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

#include <robowflex_dart/ActionRobot.h>
#include <robowflex_dart/urdf_read.h>

#include <robowflex_library/util.h>
#include <robowflex_library/class_forward.h>
#include <robowflex_dart/gui.h>
#include <robowflex_dart/planning.h>
#include <robowflex_dart/robot.h>
#include <robowflex_dart/world.h>
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
        env_name = "grid_world"; // test in cpp


    /* NEVER CHANGE THIS ROBOT LOADING STRUCTURE */
    auto fetch_dart = darts::loadMoveItRobot("fetch",                                         //
                                             abs_path +"envs/fetch/urdf/fetch6.urdf",  //
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
        std::vector<ActionP> actions_path;
        std::vector<ActionR> actions_robot;
        getActionsFromPath(env_name, input_.group_indices, actions_path);
        translateActions(actions_path, obj_, actions_robot);

        darts::PlanBuilder builder(world);
        builder.addGroup("fetch", GROUP_X);
        //builder.setStartConfiguration({0.05, 1.32, 1.4, -0.2, 1.72, 0, 1.66, 0, 0.0,0.0,0.0}); // folded arm maybe necessary
        builder.setStartConfigurationFromWorld();
        Eigen::VectorXd start_config = builder.getStartConfiguration();


//        int surface_no = 4; //maze vertical surface number
        int surface_no = 5; // regular mazes

//        world->getRobot("fetch")->setJoint("move_x_axis_joint",1.0);
//        world->getRobot("fetch")->setJoint("move_y_axis_joint",1.1);
      //  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        size_t i = 0;

        Eigen::VectorXd backup_state_fetch(int(world->getRobot(fetch_dart->getName())->getGroupJoints(GROUP_X).size()));

        world->getRobot(fetch_name)->getGroupState(GROUP_X, backup_state_fetch);
        while (i < actions_robot.size())
        {

            Eigen::VectorXd old_config = builder.getStartConfiguration();
            if (plan_to_grasp(world, window, obj_[actions_robot[i].obj_index], surface_no, true, fetch_dart,backup_state_fetch))
            {
                if(plan_to_move(world, window, obj_[actions_robot[i].obj_index], actions_robot[i], fetch_dart, maze_dart,surface_no))
                {
                    builder.setStartConfigurationFromWorld();
                    start_config = builder.getStartConfiguration();
                    world->getRobot(fetch_name)->getGroupState(GROUP_X, backup_state_fetch);
                    i++;

                }else{
                    world->getRobot(fetch_name)->setGroupState(GROUP_X,backup_state_fetch);
                }
            }else{
                world->getRobot(fetch_name)->setGroupState(GROUP_X,backup_state_fetch);
            }
//                start_config = old_config;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    });

    return 0;
}
