/// @file
///
/// @brief Implementation of the Simulation class which coordinates all
/// calculations.
///

#include <iostream>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <cassert>
#include <sstream>

// nanoflann library for distance computations
#include "nanoflann.hpp"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>

#include "simulation.hpp"

// define globals
const int Simulation::kDims = DIMENSIONS;
const std::string Simulation::kParamsFilename = "params.txt";
/// small epsilon used in distance calculations, float so added f
const float Simulation::kSpatialEpsilon = 0.001f;

/// @brief Constructor starts simulation.
///
Simulation::Simulation() {
    run_simulation();
}

/// @brief Extracts parameters from params file.
///
/// @returns params_map: map that contains params std::string key value pairs
///
std::map<std::string, std::string> Simulation::read_params_file() const {
    std::map<std::string, std::string> params_map;
    
    std::ifstream params_file;
    params_file.open(kParamsFilename);
    if (params_file.fail()) {
	std::cout << "File " << kParamsFilename << " not found!" << std::endl;
	exit(-1);
    }
    std::string line;
    std::string type;
    std::string name;
    std::string dummy;
    std::string value;
    // param format is "type name = value"
    // ignore type, just for reference, parse as strings for now
    // order in file does not matter
    while (std::getline(params_file, line)) {
	if (!line.empty() && line[0] != '#') {
	    std::istringstream iss(line);
	    iss >> type >> name >> dummy >> value;
	    params_map[name] = value;
	}
    }
    std::cout << "# Params" << std::endl;
    for (const auto &k_v_pair : params_map ) {
        std::cout << "# " << k_v_pair.first << " = " << k_v_pair.second <<
	    std::endl;
    }
    std::cout << std::endl;
    return params_map;
}

/// @brief Defines simulation parameters in member variables. 
/// 
/// @returns initial_N_ions: number of ions initially in box (sei + bath)
///
std::string Simulation::initialize_params() {
    // parse everything from file as std::strings
    std::map<std::string, std::string> params_map = read_params_file();

    std::string restart_path;
    if (params_map.find("restart_path") != params_map.end()) {
	restart_path = params_map["restart_path"];
    }
    else {
	restart_path = "";
    }

    write_frame_interval_ = std::stoi(params_map["write_frame_interval"]);
    cluster_size_ = static_cast <int> (std::stof(params_map["cluster_size"]));
    max_leaf_size_ = std::stoi(params_map["max_leaf_size"]);
    seed_ = std::stoi(params_map["seed"]);


    // set expected length of particle movement in 2d or 3d
    float rms_jump_size = std::stof(params_map["rms_jump_size"]);
    // in one dimensional Brownian motion, the rms length of paticle step is 
    // sqrt(2 * D * dt_), in reduced units the D disappears 
    // so
    // rms_jump_size = sqrt(2 * kDims * dt_)
    dt_ = std::pow(rms_jump_size, 2) / (2 * kDims);

    float fraction_max_kappa = std::stof(params_map["fraction_max_kappa"]);
    // this line is confusing,
    // 
    // p_max = kappa_max * sqrt(dt) = 1.0 
    // kappa_max = p_max / sqrt(dt) = 1.0 / sqrt(dt)
    // p_ = kappa * sqrt(dt) = fraction_max_kappa * max_kappa * sqrt(dt)
    // p_ = fraction_kappa_max * 1.0 / sqrt(dt) * sqrt(dt)
    //
    // so finally
    p_ = fraction_max_kappa;
    std::cout << "p_ = fraction_max_kappa = " << p_ << std::endl;
    
    // kappa = p_ / sqrt(dt) 
    float kappa = p_ / std::sqrt(dt_);
    std::cout << "da = kappa^2 = " << std::pow(kappa, 2) << std::endl;

    // set jump_cutoff_ for cell based spatial parallelism
    jump_cutoff_ = std::stof(params_map["jump_cutoff"]);
    // cell length must be larger than maximum jump size + 2 * diameter + 
    // epsilon so that all collisions can be resolved
    int diameter = 2;
    float max_jump_length = std::sqrt(2 * kDims * dt_) * jump_cutoff_;
    std::cout << "max_jump_length = " << max_jump_length << std::endl;
    cell_length_ = max_jump_length + diameter + kSpatialEpsilon;
    std::cout << "cell_length_ = " << cell_length_ << std::endl;
    
    return restart_path;
}

/// @brief Sets up plated vector and kd tree.
///
/// @param[out] plated: vector of plated vectors
/// @param[out] kd_tree: kd tree of plated
///
/// @returns radius: cluster radius, distance from origin to furthest plated
///
int Simulation::set_up_structures(std::string restart_path, 
				   std::vector<std::vector<float>> & plated,
				   KDTree & kd_tree) {
    float radius;
    if (!restart_path.empty()) {
	std::cout << "Restarting from file " << restart_path << std::endl;
        // load_state(setup_params.restart_path);
    }
    else {
        // add single plated at origin to start
        plated.resize(1);
        plated.at(0).resize(kDims);
        for (int i = 0; i < kDims; i++) {
            plated.at(0).at(i) = 0;
        }
        (*kd_tree.index).addPoints(0, 0);
	radius = 0;
    }
    return radius;
}

/// @brief Finds a random point on a ball with given radius and dimension.
///
/// @param kDims: the dimensions of the ball
/// @param radius: the radius of the ball
///
/// @returns result: vector representing random point
///
std::vector<float> Simulation::generate_point_on_ball(int kDims, 
						      float radius) {
    std::vector<float> result(kDims);
    for (int i = 0; i < kDims; i++) {
	result.at(i) = 1.0;
    }
    return result;
}

/// @brief Coordinates running simulation.
///
void Simulation::run_simulation() {
    // get simulattion parameters
    std::cout << "DIMENSION = " << kDims << std::endl;
    std::string restart_path = initialize_params();
    // build data structures to track plated
    std::vector<std::vector<float>> plated;
    KDTree kd_tree(kDims, plated, max_leaf_size_);
    // tracks radius of cluster, distance from origin to furthest plated
    int radius = set_up_structures(restart_path, plated, kd_tree);
    // set up rng
    // ?
    
    // tracks whether current particle has stuck
    bool stuck = false;
    // main loop
    for (int i = 0; i < cluster_size_; i++) {
	std::vector<float> particle = generate_point_on_ball(kDims, radius);
	while (!stuck) {
	    // do a k nearest neighbors search with k = 1
	    const size_t k = 1;
	    size_t index;
	    float squared_distance;
	    kd_tree.query(&particle[0], k, &index, &squared_distance);
	    if (squared_distance > std::pow(cell_length_, 2)) {
		// Take as large a step as possible
	    }
	    else {
		// evaluate collision
		// bounce or plate particle
	    }
	    // see if particle crossed boundary, if it did regenerate it
	    exit(0);
	}
    }
}

/// @brief Blank destructor.
///
Simulation::~Simulation() {}
