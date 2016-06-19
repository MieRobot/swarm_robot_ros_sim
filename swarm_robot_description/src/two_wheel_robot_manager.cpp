// this node manage the number and index of present two wheel robot

// publish topic on two wheel robot information, including:
    // robot index, 2D position, orientation and speed of two wheels
// use gazebo service to add and delete model in gazebo
// (compatible when a robot model is deleted directly from gazebo gui)
// accept service request the command of adding or deleting robot model
// (adding or deleting action will be reflected directly in the topic msg)

// this node is not compatible with different robot models, for two wheel robot only
// because only two wheel robot has wheel speed, may write another node for other robot models

// both low and high level robot control subscribe to same robot information topic
// although it's better to divide the message into two, and publish at different frequency
// this one topic method can be reused when it comes to other robot models




// check collision when adding robots




#include <ros/ros.h>
#include <swarm_robot_msgs/two_wheel_robot.h>
#include <swarm_robot_srv/two_wheel_robot_update.h>
#include <gazebo_msgs/SpawnModel.h>
#include <gazebo_msgs/DeleteModel.h>
#include <gazebo_msgs/ModelStates.h>
#include <gazebo_msgs/GetJointProperties.h>
#include <geometry_msgs/Quaternion.h>
#include <gaometry_msgs/Pose.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdlib.h>  // rand()

// global variables
// THE container maintained by this node
swarm_robot_msgs::two_wheel_robot current_robots;
bool robot_position_updated = false;

// callback for getting robot positions
// also check and update if there is any addition or deletion of robots
void modelStatesCallback(const gazebo_msgs::ModelStates& current_model_states) {
    // parsing the two wheel robots from all the models in gazebo
    // possible that there are models for obstacles or other environments
    int model_quantity = current_model_states.name.size();
    // the index of robots in the container
    std::vector<int32_t> container_index = current_robots.index;
    for (int i=0; i<model_quantity; i++) {
        // check if it is a two wheel robot
        // there is a underscore between the name and the index
        std::size_t found = current_model_states.name[i].find("two_wheel_robot");
        if (found != std::string::npos) {
            // a two_wheel_robot has been found
            // get the robot index
            // 16 = 15 + 1, 15 is the length of "two_wheel_robot"
            std::string index_str = current_model_states.name[i].substr(16);
            int index_parsed = std::atoi(index_str.c_str());
            // search in the container
            int container_size = container_index.size();
            bool parsed_index_found = false;
            for (int j=0; j<container_size; j++) {
                // the size of container_index may change for each i
                if (index_parsed == container_index[j]) {
                    // update the 2D position of two wheel robots
                    current_robots.x[j] = current_model_states.pose[i].position.x;
                    current_robots.y[j] = current_model_states.pose[i].position.y;
                    current_robots.orientation[j]
                        = quaternion_to_angle(current_model_states.pose[i].orientation);
                    container_index.erase(j);
                    parsed_index_found = true;
                    break;
                }
            }
            if (!parsed_index_found) {
                // parsed index not found in the container, ADDITION found!
                // update a new robot in the container
                current_robots.index.push_back(index_parsed);
                current_robots.x.push_back(current_model_states.pose[i].position.x);
                current_robots.y.push_back(current_model_states.pose[i].position.y);
                current_robots.orientation.push_back(quaternion_to_angle(current_model_states.pose[i].orientation));
                current_robots.left_wheel_vel.push_back(0);
                current_robots.right_wheel_vel.push_back(0);
                ROS_INFO_STREAM("robot addition detected: two_wheel_robot_" << index_str);
            }
        }
    }

    // update the container if there is deletion in gazebo
    if (container_index.size() != 0) {
        int container_index_size = container_index.size();
        for (int i=0; i<container_index_size; i++) {
            int current_robots_index_size = current_robots.index.size();
            for (int j=0; j<current_robots_index_size; j++) {
                // the index should be found before this loop ends
                if (container_index[i] == current_robots.index[j]) {
                    // erase the node
                    current_robots.index.erase(j);
                    current_robots.x.erase(j);
                    current_robots.y.erase(j);
                    current_robots.orientation.erase(j);
                    current_robots.left_wheel_vel.erase(j);
                    current_robots.right_wheel_vel.erase(j);
                    ROS_INFO_STREAM("robot deletion detected: two_wheel_robot_"
                        << intToString(container_index[i]));
                    break;
                }
            }
        }
    }

    // reset robot position updated flag
    robot_position_updated = true;
}

// callback for service to change robot models in gazebo
bool twoWheelRobotUpdateCallback(swarm_robot_srv::two_wheel_robot_updateRequest& request
    , swarm_robot_srv::two_wheel_robot_updateResponse& response
    , ros::ServiceClient add_model_client
    , ros::ServiceClient delete_model_client) {
    ROS_INFO_STREAM("in the two wheel robot update callback");
    // add or delete models in gazebo here
    if (request.update_code < swarm_robot_srv::two_wheel_robot_updateRequest::DELETE_ALL) {
        // update code is a negative number, meaning delete a number of robots
        std::vector<int32_t> container_index = current_robots.index;
        int delete_robot_quantity = std::abs(request.update_code);
        int current_robot_quantity = container_index.size();
        if (delete_robot_quantity > current_robot_quantity) {
            ROS_ERROR("requested deletion quantity exceeds existed robots");
            response.response_code
                = swarm_robot_srv::two_wheel_robot_updateResponse::DELETE_FAIL_EXCEED_QUANTITY;
            return false;
        }
        else {
            // start randomly choose robot to delete
            gazebo_msgs::DeleteModel delete_model_srv_msg;
            for (int i=0; i<delete_robot_quantity; i++) {
                // repeat random deletion for "delete_robot_quantity" times
                // index of the vector "container_index"
                int container_index_index = rand() % container_index.size();
                int delete_index = container_index[container_index_index];
                container_index.erase(container_index_index);  // avoid deletion on same robot
                delete_model_srv_msg.model_name = "two_wheel_robot_" + intToString(delete_index);
                bool call_service = delete_model_client.call(delete_model_srv_msg);
                if (call_service) {
                    if (delete_model_srv_msg.response.success) {
                        ROS_INFO_STREAM("two_wheel_robot_" << intToString(delete_index)
                            << " has been deleted");
                    }
                    else {
                        ROS_INFO_STREAM("two_wheel_robot_" << intToString(delete_index)
                            << " deletion failed");
                    }
                }
                else {
                    ROS_ERROR("fail to connect with gazebo server");
                    response.response_code
                        = swarm_robot_srv::two_wheel_robot_updateResponse::DELETE_FAIL_NO_RESPONSE;
                    return false;
                }
            }
        }
    }
    else if (request.update_code == swarm_robot_srv::two_wheel_robot_updateRequest::DELETE_ALL) {
        // delete all robots sequentially, no need to randomly choose

    }
}

// quaternion => rotation angle of two wheel robot
double quaternion_to_angle(geometry_msgs::Quaternion input_quaternion) {
    // this assume the x and y element of the quaternion is close to zero
    return atan(input_quaternion.z/input_quaternion.w) * 2
}

// int to string converter
std::string intToString(int a) {
    std::stringstream ss;
    ss << a;
    return ss.str();
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "/swarm_sim/two_wheel_robot_manager");
    ros::NodeHandle nh;

    // handshake with robot name in parameter server, and get model urdf
    std::string robot_name;
    std::string two_wheel_robot_urdf;
    bool get_name, get_urdf;
    get_name = nh.getParam("/swarm_sim/robot_name", robot_name);
    get_urdf = nh.getParam("/swarm_sim/two_wheel_robot_urdf", two_wheel_robot_urdf);
    if (!(get_name && get_urdf)) {
        ROS_ERROR("parameter server is not set");
        return 0;  // return when parameter server is not good
    }
    if (robot_name != "two_wheel_robot") {
        ROS_ERROR("wrong robot according to parameter server");
        return 0;  // return when wrong robot manager is called
    }

    // check if gazebo is up and running by check service "/gazebo/set_physics_properties"
    // this service seems like the last service hosted by gazebo
    ros::Duration half_sec(0.5);
    bool gazebo_ready = ros::service::exist("/gazebo/set_physics_properties", true);
    if (!gazebo_ready) {
        // gazebo not ready
        while (!gazebo_ready) {
            ROS_INFO("waiting for gazebo");
            half_sec.sleep();
            gazebo_ready = ros::service::exist("/gazebo/set_physics_properties", true);
        }
    }

    // initialize THE container
    current_robots.index.clear();
    current_robots.x.clear();
    current_robots.y.clear();
    current_robots.orientation.clear();
    current_robots.left_wheel_vel.clear();
    current_robots.right_wheel_vel.clear();

    // instantiate a publisher for the managed information of two wheel robot
    ros::Publisher two_wheel_robot_publisher
        = nh.advertise<swarm_robot_msgs::two_wheel_robot>("/swarm_sim/two_wheel_robot", 1);

    // instantiate a subscriber for "/gazebo/model_states"
    ros::Subscriber model_states_subscriber
        = nh.subscribe("/gazebo/model_states", 1, modelStatesCallback);
    // this topic publish at 1000hz rate

    // instantiate a service client to get wheel velocities
    ros::ServiceClient joint_properties_client = nh.serviceClient<gazebo_msgs::GetJointProperties>(
        "/gazebo/get_joint_properties");
    gazebo_msgs::GetJointProperties joint_properties_srv_msg;

    // instantiate a service server to modify the robots in gazebo
    // add or delete robot models in gazebo
    ros::ServiceServer two_wheel_robot_service
        = nh.advertiseService("/swarm_sim/two_wheel_robot_update", twoWheelRobotUpdateCallback);

    // instantiate a service client for "/gazebo/spawn_urdf_model"
    ros::ServiceClient add_model_client
        = nh.serviceClient<gazebo_msgs::SpawnModel>("/gazebo/spawn_urdf_model");
    gazebo_msgs::SpawnModel add_model_srv_msg;  // service message
    geometry_msgs::Pose model_pose;  // pose message for service message

    // instantiate a service client for "/gazebo/delete_model"
    ros::ServiceClient delete_model_client
        = nh.serviceClient<gazebo_msgs::DeleteModel>("/gazebo/delete_model");




    // publish loop



}







