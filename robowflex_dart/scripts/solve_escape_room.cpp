
//
// Created by serboba on 25.03.22.
//

//
// Created by serboba on 15.12.21.
//

//
// Created by serboba on 06.12.21.
//


//
// Created by serboba on 12.10.21.
//

//
// Created by serboba on 05.09.21.
//

#include <chrono>
#include <thread>

// #include <robowflex_dart/LARRT.h>

#include <ompl/geometric/planners/rrt/LARRT.h>
#include <ompl/base/objectives/MinimalActionsObjective.h>

#include <robowflex_library/builder.h>
#include <robowflex_library/log.h>
#include <robowflex_library/robot.h>
#include <robowflex_library/scene.h>
#include <robowflex_library/util.h>
#include <robowflex_dart/gui.h>
#include <robowflex_dart/planning.h>
#include <robowflex_dart/robot.h>
#include <robowflex_dart/tsr.h>
#include <robowflex_dart/world.h>
#include <robowflex_dart/point_collector.h>
#include <robowflex_dart/urdf_read.h>

boost::filesystem::path p(boost::filesystem::current_path().parent_path().parent_path().parent_path());
const std::string abs_path = p.string() + "/src/robowflex/robowflex_dart/include/io/";

using namespace robowflex;

static const std::string GROUP = "arm_with_torso";

int main(int argc, char **argv)
{
    // Startup ROS
    ROS ros(argc, argv);

    double time;
    std::string env_name;
    if(argc > 1 )
    {
        env_name = std::string(argv[1]);
        time = atof(argv[2]);
    }
    else
    {
        env_name = "room0"; // room0 or room1 / escape room, room2 limitation demonstration
        time = 300;
    }

    std::string fetch_name;

    if(env_name == "room0" || env_name == "room1")
        fetch_name = "fetch3";

    else if(env_name =="room2" || env_name == "room3")
        fetch_name = "fetch2";

    auto fetch_dart = robowflex::darts::loadMoveItRobot("fetch",                                         //
                                                        abs_path +"envs/fetch/urdf/" + fetch_name + ".urdf",
                                                        abs_path +"envs/fetch/srdf/" + "fetch3" + ".srdf");


    auto room_dart = robowflex::darts::loadMoveItRobot("a1",
                                                       abs_path +"envs/" + env_name+ "/" + "urdf/" + env_name + ".urdf",
                                                       abs_path +"envs/" + env_name+ "/" + "srdf/" + env_name + ".srdf");


    auto world = std::make_shared<robowflex::darts::World>(); // achtung, world->robots_ is std map, automatically sorts robots by their names
    world->addRobot(room_dart);
    world->addRobot(fetch_dart);




    darts::Window window(world);

    const auto &plan_solution_all = [&]() {

        URDF_IO input_(env_name);

        int lastIndex = input_.group_indices.back().back();
        std::vector<int> robot_gr = {lastIndex+1,lastIndex+2};
        input_.group_indices.push_back(robot_gr);

        int goal_index = input_.group_indices.size()-1;

        darts::PlanBuilder builder(world,input_.group_indices); // using my statespace

        for(std::string group : input_.group_names) {
            builder.addGroup("a1",group);
        }

        builder.addGroup("fetch","move_robot");


        world->getRobot("fetch")->setJoint("torso_lift_joint",0.05);
        world->getRobot("fetch")->setJoint("shoulder_pan_joint",1.32);
        world->getRobot("fetch")->setJoint("shoulder_lift_joint",1.4);
        world->getRobot("fetch")->setJoint("upperarm_roll_joint",-0.2);
        world->getRobot("fetch")->setJoint("elbow_flex_joint",1.72);
        world->getRobot("fetch")->setJoint("forearm_roll_joint",0);
        world->getRobot("fetch")->setJoint("wrist_flex_joint",1.66);
        world->getRobot("fetch")->setJoint("wrist_roll_joint",0);


        if(env_name == "room2" || env_name == "room3")  // start position is outside the room
        {
            world->getRobot("fetch")->setJoint("move_x_axis_joint",2.2);
            world->getRobot("fetch")->setJoint("move_y_axis_joint",-1.0);

        }else if(env_name=="room1")
        {
            world->getRobot("fetch")->setJoint("move_y_axis_joint",0.7);
            world->getRobot("fetch")->setJoint("move_x_axis_joint",-0.6);
        }

        // ADD ALL GROUPS THAT ARE NEEDED

        builder.setStartConfigurationFromWorld();
        builder.initialize();


        std::this_thread::sleep_for(std::chrono::seconds(30));
        darts::TSR::Specification goal_spec;
        goal_spec.setFrame("fetch", "base_link", "move_x_axis");
        goal_spec.setPose(input_.goal_pose);
        auto goal_tsr = std::make_shared<darts::TSR>(world, goal_spec);
        auto goal = builder.getGoalTSR(goal_tsr);

        builder.setGoal(goal);


        builder.ss->setOptimizationObjective(std::make_shared<ompl::base::MinimalActionsObjective>(builder.info,input_.group_indices));
        auto planner = std::make_shared<ompl::geometric::LARRT>(builder.info, input_.group_indices, true, goal_index); // last parameter is state isolation
//      auto planner = std::make_shared<ompl::geometric::RRTstar>(builder.info);
//      auto planner = std::make_shared<ompl::geometric::BITstar>(builder.info);
//     auto planner = std::make_shared<ompl::geometric::RRTConnect>(builder.info,false);

        builder.ss->setPlanner(planner);
        builder.setup();

        builder.space->sanityChecks();
        builder.rspace->sanityChecks();

        goal->startSampling();
        ompl::base::PlannerStatus solved = builder.ss->solve(time);
        goal->stopSampling();

        if (solved)
        {
            ompl::geometric::PathGeometric path(builder.getSolutionPath(false,false));
            //path.interpolate() if not rrtnew maybe

            std::string file_name = abs_path +"path_result/"+env_name + ".txt";
            std::ofstream fs(file_name);
            path.printAsMatrix(fs);
            window.animatePath(builder, path,50,1);
        }
        else
            RBX_WARN("No solution found");
    };

    window.run([&] {
        //   std::this_thread::sleep_for(std::chrono::milliseconds(200000));
        plan_solution_all();
    });

    return 0;
}

