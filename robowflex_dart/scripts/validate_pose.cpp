//
// Created by serboba on 20.01.22.
//

//
// Created by serboba on 03.12.21.
//

//
// Created by serboba on 13.10.21.
//

//
// Created by serboba on 05.09.21.
//

//
// Created by serboba 19.11.21.
//

#include <chrono>
#include <thread>

#include <ompl/geometric/planners/rrt/RRTConnect.h>
#include <ompl/geometric/planners/kpiece/KPIECE1.h>
//#include <ompl/geometric/planners/rrt/RRT.h>
//#include <ompl/geometric/planners/sst/SST.h>
#include <robowflex_dart/ActionRobot.h>
#include <robowflex_dart/urdf_read.h>

#include <robowflex_library/builder.h>
#include <robowflex_library/log.h>
#include <robowflex_library/robot.h>
#include <robowflex_library/scene.h>
#include <robowflex_library/util.h>
#include <ompl/base/goals/GoalLazySamples.h>
#include <robowflex_library/class_forward.h>
#include <robowflex_dart/gui.h>
#include <robowflex_dart/planning.h>
#include <robowflex_dart/robot.h>
#include <robowflex_dart/tsr.h>
#include <robowflex_dart/world.h>

#include <robowflex_dart/point_collector.h>
#include <robowflex_dart/conversion_functions.h>
#include <robowflex_dart/quaternion_factory.h>
#include <robowflex_dart/Object.h>
#include <robowflex_dart/planningFunctions.h>

using namespace robowflex;

boost::filesystem::path p(boost::filesystem::current_path().parent_path().parent_path().parent_path());
const std::string abs_path = p.string() + "/src/robowflex/robowflex_dart/include/io/";


static const std::string GROUP_X = "arm_with_x_move";



int main(int argc, char **argv)
{
    // Startup ROS
    ROS ros(argc, argv);

    std::string env_name;
    if(argc > 1 )
        env_name = std::string(argv[1]);
    else
        env_name = "room0"; // test in cpp


    /* NEVER CHANGE THIS ROBOT LOADING STRUCTURE */
    auto fetch_dart = darts::loadMoveItRobot("fetch",                                         //
                                             abs_path +"envs/fetch/urdf/fetch4.urdf",  //
                                             abs_path +"envs/fetch/srdf/fetch4.srdf");


    auto maze_dart = darts::loadMoveItRobot(env_name,
                                            abs_path +"envs/" + env_name+ "/" + "urdf/" + env_name + ".urdf",
                                            abs_path +"envs/" + env_name+ "/" + "srdf/" + env_name + ".srdf");

    auto fetch_name = fetch_dart->getName();
    auto door_name = maze_dart->getName();
    auto world = std::make_shared<darts::World>();
    world->addRobot(fetch_dart);
    world->addRobot(maze_dart);

    create_txt_from_urdf(env_name);
    std::vector<Object> obj_;
    read_obj_txt_file(env_name,obj_);

    /* NEVER CHANGE THIS ROBOT LOADING STRUCTURE UNTIL HERE !!!! */
    darts::Window window(world);

    // fetch_dart->setJoint("torso_lift_joint",0.25); // maze2 to avoid start collision
    // fetch_dart->setJoint("move_x_axis_joint",-0.10);
    Eigen::VectorXd start(11);
    start << 0.05, 1.32, 1.4, -0.2, 1.72, 0, 1.66, 0, 0.0,0.0,0.0;
    fetch_dart->setGroupState("arm_with_x_move",start);

    window.run([&] {

        URDF_IO input_(env_name);
        int lastIndex = input_.group_indices.back().back();
        std::vector<int> robot_gr = {lastIndex+1,lastIndex+2};
        input_.group_indices.push_back(robot_gr);

        darts::PlanBuilder builder(world);
        builder.addGroup("fetch", GROUP_X);
        //builder.setStartConfiguration({0.05, 1.32, 1.4, -0.2, 1.72, 0, 1.66, 0, 0.0,0.0,0.0}); // folded arm maybe necessary
        builder.setStartConfigurationFromWorld();
        Eigen::VectorXd start_config = builder.getStartConfiguration();
        // int surface_no = 4; //maze vertical surface number
        int surface_no = 5; // regular mazes

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

        size_t i = 5;


            if (plan_to_grasp(world, window, obj_[0], surface_no, true, fetch_dart,start_config))
            {
                Vector3d pos;
                Vector3d rpy;
                std::string s = "link_0_joint_0";
                pos << 0.0,0.0,0.0;
                rpy << 0,0,1.50;
//                rpy << 1.50,0,0;
                if(plan_to_move(world, window, obj_[0],ActionR(pos,rpy,s, 0), fetch_dart, maze_dart,surface_no))
                {
                }
            }

    });

    return 0;
}
