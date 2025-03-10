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

#include <ompl/base/objectives/MinimalActionsObjective.h>
#include <ompl/base/spaces/FragmentedStateSpace.h>
#include <ompl/geometric/planners/rrt/LARRT.h>

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
        env_name = "grid_world"; // test in cpp
        time = 30;
    }
    auto maze_dart = darts::loadMoveItRobot(env_name,
                                            abs_path +"envs/" + env_name+ "/" + "urdf/" + env_name + ".urdf",
                                            abs_path +"envs/" + env_name+ "/" + "srdf/" + env_name + ".srdf");

    auto maze_name = maze_dart->getName();
    auto world = std::make_shared<darts::World>();
    world->addRobot(maze_dart);

    darts::Window window(world);


    const auto &plan_solution_all = [&]() {

        URDF_IO input_(env_name);

        darts::PlanBuilder builder(world); // using my statespace

        builder.space = std::make_shared<ompl::base::FragmentedStateSpace>(input_.group_indices);
        for(std::string group : input_.group_names) {
            builder.addGroup(maze_name,group);
        }
        // ADD ALL GROUPS THAT ARE NEEDED

        builder.setStartConfigurationFromWorld();

        builder.initialize();


        darts::TSR::Specification goal_spec;
        goal_spec.setFrame(maze_name, "link_0_joint_0", "base_link");
        goal_spec.setPose(input_.goal_pose);
        auto goal_tsr = std::make_shared<darts::TSR>(world, goal_spec);
        auto goal = builder.getGoalTSR(goal_tsr);

        builder.setGoal(goal);


        builder.ss->setOptimizationObjective(std::make_shared<ompl::base::MinimalActionsObjective>(builder.info,input_.group_indices));
        auto planner = std::make_shared<ompl::geometric::LARRT>(builder.info, input_.group_indices, true); // last parameter is state isolation

        builder.ss->setPlanner(planner);
        builder.setup();

        builder.space->sanityChecks();
        builder.rspace->sanityChecks();

        goal->startSampling();
        ompl::base::PlannerStatus solved = builder.ss->solve(time);
        goal->stopSampling();

        builder.getSpace()->printSettings(std::cout);
        if (solved)
        {
            ompl::geometric::PathGeometric path(builder.getSolutionPath(false,false));
            //path.interpolate() if not rrtnew maybe

            std::string file_name = abs_path +"path_result/"+env_name + ".txt";
            std::ofstream fs(file_name);
            path.printAsMatrix(fs);
            window.animatePath(builder, path,10,1);
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

