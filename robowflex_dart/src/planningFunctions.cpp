//
// Created by serboba on 05.01.22.
//

#include <robowflex_dart/planningFunctions.h>

#include <ompl/base/objectives/PathLengthOptimizationObjective.h>
#include <ompl/base/objectives/MinimaxObjective.h>
#include <ompl/base/objectives/StateCostIntegralObjective.h>


#include <ompl/geometric/PathSimplifier.h>
#include <ompl/geometric/PathGeometric.h>

#include <ompl/geometric/PathHybridization.h>
using namespace robowflex;

static const std::string GROUP_X = "arm_with_x_move";



bool plan_to_fold_arm(std::shared_ptr<darts::World> &world, darts::Window &window ) { // new position =  world position
    darts::PlanBuilder builder(world);
    builder.addGroup("fetch", GROUP_X);
    builder.setStartConfigurationFromWorld();
    auto start_config = builder.getStartConfiguration();
    builder.initialize();

    auto goal = builder.getGoalConfiguration({0.05, 1.32, 1.4, -0.2, 1.72, 0, 1.66, 0, start_config[8],start_config[9],start_config[10] });
    builder.setGoal(goal);
    auto rrt = std::make_shared<ompl::geometric::RRTConnect>(builder.info,true);
    builder.ss->setPlanner(rrt);
    builder.setup();
    ompl::base::PlannerStatus solved = builder.ss->solve(20.0);


    if (solved == ompl::base::PlannerStatus::EXACT_SOLUTION )
    {
        RBX_INFO("Found solution!");
        window.animatePath(builder, builder.getSolutionPath());
        return true;
    }
    else{
        RBX_WARN("No solution found");
        return false;
    }
};


void strech_normal(Eigen::MatrixXd &normal_vec){ // add normal to the pos we calculated because wrist not positioned right
    for(int i = 0; i < 3 ;i++){ // TODO NORMAL DOWNWARDS (NOW UPWARDS) IF BOTH CORRECT OR ONE CORRECT ITS THE RIGHT POSITION BUT FIRST GO UP
        if(normal_vec(0,i) == 0.0)
            continue;
        if(normal_vec(0,i) > 0.0)
            normal_vec(0,i) +=0.01;
        else
            normal_vec(0,i) -=0.01;
    }

}

void get_start_state (darts::PlanBuilder &builder_, std::vector<double> &config){ // to secure the start state
    Eigen::VectorXd start_config = builder_.getStartConfiguration();
    config.resize(start_config.size());
    Eigen::VectorXd::Map(&config[0], start_config.size()) = start_config;

}

void set_start_grasp_pos(darts::PlanBuilder &builder_,std::shared_ptr<darts::World> &world,
                         Eigen::MatrixXd new_position,Eigen::MatrixXd quaternion){

    darts::TSR::Specification start_spec;
    start_spec.setFrame("fetch","gripper_link","move_x_axis");
    start_spec.setPosition(new_position(0),new_position(1),new_position(2));
    start_spec.setRotation(quaternion(0),quaternion(1),quaternion(2),quaternion(3));

    auto start_tsr = std::make_shared<darts::TSR>(world,start_spec);
    start_tsr->initialize();
    start_tsr->useGroup(GROUP_X);
    start_tsr->solveWorld();

    builder_.setStartConfigurationFromWorld();
    builder_.initialize();
}

void set_goal_from_world(darts::PlanBuilder &builder_,std::shared_ptr<darts::World> &world){
    darts::TSR::Specification goal_spec2;  //
    goal_spec2.setFrame("fetch", "wrist_roll_link", "move_x_axis");
    goal_spec2.setPoseFromWorld(world);
    auto goal_tsr2 = std::make_shared<darts::TSR>(world, goal_spec2);
    auto goal = builder_.getGoalTSR(goal_tsr2);
    goal->setThreshold(0.0001);
    builder_.setGoal(goal);
}

bool evaluate_pos (std::shared_ptr<darts::Robot> robot_,Eigen::MatrixXd &pose, bool normals_help = true){

    auto world = std::make_shared<darts::World>();

    world->addRobot(robot_);

    darts::PlanBuilder builder(world);
    builder.addGroup("fetch", GROUP_X);

    bool found_pose = false;
    Eigen::MatrixXd normal_v = pose.block(0, 7, 1, 3);
    Eigen::MatrixXd position = pose.block(0, 0, 1, 3);
    Eigen::MatrixXd quaternion = pose.block(0, 3, 1, 4);
    double l = 0.0;

    while((!found_pose && l<=0.20))
    { // HOW TO SET L VALUE?? l = stretching factor for normal // new -> disable/enable strech normal depending on what obj you grasp
        l = l+ 0.01; // todo adjust max normal value?

//        if(normals_help)
//            strech_normal(normal_v);

        std::vector<double> start_vec;
        get_start_state(builder,start_vec); // get initial (state) position of the robot currently

        Eigen::MatrixXd new_position = position + normal_v; // adjust the pose with the normal
        set_start_grasp_pos(builder,world,new_position,quaternion); // solve the grasping point with INVERSE KINEMATICS

        std::vector<double> config_vec;
        get_start_state(builder,config_vec);  // get the state we found throughout the inverse kinematics solution
        set_goal_from_world(builder,world); // set the goal so that we can call simplesetup but unnecessary

        auto rrt = std::make_shared<ompl::geometric::RRTConnect>(builder.info,true); // unnecessary but to collision check builder must be setup
        builder.ss->setPlanner(rrt);
        builder.setup();


        ompl::base::State * goal_state = builder.rspace->allocState();
        builder.space->copyFromReals(goal_state,config_vec); // copy the IK solution into ompl state

        if (builder.rinfo->isValid(goal_state)){
            found_pose=true;
            OMPL_DEBUG("TRUE STATE VALID");
            Eigen::MatrixXd result(1,7);
            result << new_position,quaternion;
            pose = result;
            builder.rspace->freeState(goal_state);
            return true;
        }else{
            //   OMPL_DEBUG("FALSE INVALID STATE");
        }
    }
    return false;
}


bool grasp(std::shared_ptr<darts::World> &world, darts::Window &window, Object &obj, int &surf_no,
           bool normals_help, std::shared_ptr<darts::Robot> &robot_, Eigen::VectorXd old_config){

    Eigen::MatrixXd pose_m = get_pose_object(obj,surf_no);
    evaluate_pos(robot_,pose_m, normals_help);
    world->getRobot(robot_->getName())->setGroupState(GROUP_X,old_config);

    darts::PlanBuilder builder(world);
    builder.addGroup(robot_->getName(), GROUP_X);
    builder.setStartConfigurationFromWorld();

    builder.initialize();

    ompl::base::PlannerStatus solved;

    darts::TSR::Specification goal_spec;
    goal_spec.setFrame(robot_->getName(), "gripper_link", "move_x_axis");
    goal_spec.setPose(pose_m); // HOP

    auto goal_tsr = std::make_shared<darts::TSR>(world, goal_spec);
    auto goal = builder.getGoalTSR(goal_tsr);

    goal->setThreshold(0.0001);
    builder.setGoal(goal);

    auto rrt = std::make_shared<ompl::geometric::RRTConnect>(builder.info,true);
    //rrt->setRange(0.01);
    builder.ss->setPlanner(rrt);
    builder.setup();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    goal->startSampling();
    solved = builder.ss->solve(2.0);
    goal->stopSampling();

    std::cout <<"1111 :" << builder.getStartConfiguration() << std::endl;
    std::cout <<"1111 :" << old_config << std::endl;
    if (solved == ompl::base::PlannerStatus::EXACT_SOLUTION){

        RBX_INFO("Found solution!");
        builder.ss->simplifySolution(10);
        builder.ss->simplifySolution(10);
        builder.ss->simplifySolution(10);
        window.animatePath(builder,  builder.getSolutionPath());
        return true;
    }else{
        RBX_WARN("No solution found");
        world->getRobot(robot_->getName())->setGroupState(GROUP_X,old_config);

        return false;
    }
}

void add_constraint_to_builder(std::shared_ptr<darts::World> &world, darts::PlanBuilder &builder, Object &obj,
                               std::shared_ptr<darts::Robot> &robot_, std::shared_ptr<darts::Robot> &obj_robot_){

    darts::TSR::Specification con_spec;
    con_spec.setBase(obj_robot_->getName(), obj.link.name);
    con_spec.setTarget(robot_->getName(),"gripper_link");
    con_spec.setPoseFromWorld(world);

    auto cons_tsr = std::make_shared<darts::TSR>(world,con_spec);
    builder.addConstraint(cons_tsr);
}

std::shared_ptr<darts::TSR> create_goalTSR(Eigen::MatrixXd rotation, Eigen::Vector3d position, std::string robot_name,
                                           std::shared_ptr<darts::World> &world){
    darts::TSR::Specification goal_spec;
    goal_spec.setFrame(robot_name,"gripper_link","move_x_axis");
    goal_spec.setPosition(position);
    goal_spec.setRotation(rotation(0),rotation(1),rotation(2),rotation(3));

    auto goal_tsr = std::make_shared<darts::TSR>(world, goal_spec);
    return goal_tsr;
}

bool plan_to_grasp(std::shared_ptr<darts::World> &world, darts::Window &window, Object &obj, int &surf_no, bool normals_help,
                   std::shared_ptr<darts::Robot> &robot_,Eigen::VectorXd start_config) {

    Eigen::VectorXd backup_state_fetch(int(world->getRobot(robot_->getName())->getGroupJoints(GROUP_X).size()));
    world->getRobot(robot_->getName())->getGroupState(GROUP_X,backup_state_fetch);

    Eigen::MatrixXd pose_m = get_pose_object(obj,surf_no);
    evaluate_pos(robot_,pose_m, normals_help);
    world->getRobot(robot_->getName())->setGroupState(GROUP_X,backup_state_fetch);

    darts::PlanBuilder builder(world);
    builder.addGroup(robot_->getName(), GROUP_X);
    builder.setStartConfigurationFromWorld();

    builder.initialize();

    ompl::base::PlannerStatus solved;

    darts::TSR::Specification goal_spec;
    goal_spec.setFrame(robot_->getName(), "gripper_link", "move_x_axis");
    goal_spec.setPose(pose_m); // HOP

    auto goal_tsr = std::make_shared<darts::TSR>(world, goal_spec);
    auto goal = builder.getGoalTSR(goal_tsr);

    goal->setThreshold(0.0001);
    builder.setGoal(goal);

    auto rrt = std::make_shared<ompl::geometric::RRTConnect>(builder.info,true);
    //rrt->setRange(0.01);
    builder.ss->setPlanner(rrt);
    builder.setup();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    goal->startSampling();
    solved = builder.ss->solve(30.0);
    goal->stopSampling();

    if (solved == ompl::base::PlannerStatus::EXACT_SOLUTION){

        RBX_INFO("Found solution!");
        builder.ss->simplifySolution(10);
        builder.ss->simplifySolution(10);
        builder.ss->simplifySolution(10);
        window.animatePath(builder,  builder.getSolutionPath());
        return true;
    }else{
        RBX_WARN("No solution found");

        world->getRobot(robot_->getName())->setGroupState(GROUP_X,backup_state_fetch);

        return false;
    }
}


void translateActionToGoalRegion(Vector3d &position,Eigen::Quaterniond &rotation,ActionR action_, Object &obj, int surface_no)
{
   // int surfno = 5; // regular maze
//    int surfno = 4; // vertical

    if(action_.pos.isZero()){ // TODO , WHAT IF OBJECT IS ALREADY ROTATED? RPY!= 0 ? AND AXIS DIFFERENT??

        std::cout << "OLDROTATION : " << rotation.w() << ", "<< rotation.x() << ", " << rotation.y() << ", " << rotation.z() << std::endl;
        std::cout << "OLDPOSITION: " << position << std::endl;

        rotation = matrix_to_quaternion(actual_quaternion(vec_to_matrix(action_.rpy+obj.actual_rotation), surface_no));
        std::cout << "ROTATION : " << rotation.w() << ", "<< rotation.x() << ", " << rotation.y() << ", " << rotation.z() << std::endl;
        Vector3d sg = action_.rpy;
        position = matrix_to_vec(get_rotated_vertex(sg, position, obj.joints.pos));
        std::cout << "POSITION: " << position << std::endl;
    }
    else  // no rotation done, just movement
    {
        position += action_.pos;
    }

}


bool plan_goal_vector(std::shared_ptr<darts::Robot> robot_, std::shared_ptr<darts::Robot> obj_robot_,Eigen::VectorXd start_config,
                      Object &obj, ActionR action_, int surface_no, std::vector<ompl::base::State *> &solution_states)
{

    auto world = std::make_shared<darts::World>();
    world->addRobot(robot_);
    world->addRobot(obj_robot_);

    darts::PlanBuilder builder(world);
    builder.addGroup(robot_->getName(), GROUP_X);
    builder.addGroup(obj_robot_->getName(), obj.group_name);
    builder.setStartConfigurationFromWorld();



    darts::TSR::Specification pos_spec;
    pos_spec.setFrame(robot_->getName(),"gripper_link", "move_x_axis");
    pos_spec.setPoseFromWorld(world);
    auto rotation = pos_spec.getRotation();
    Eigen::Vector3d position = pos_spec.getPosition();

    std::cout << position << std::endl;
    std::cout << rotation.w() << std::endl;

    add_constraint_to_builder(world,builder,obj,robot_,obj_robot_);

    builder.initialize();

    translateActionToGoalRegion(position,rotation,action_,obj,surface_no);

    auto goal = builder.getGoalTSR(create_goalTSR(eigen_quaternion_to_matrix(rotation),position,robot_->getName(),world));
    goal->setThreshold(0.001);
    builder.setGoal(goal);

    auto planner = std::make_shared<ompl::geometric::KPIECE1>(builder.info);
    builder.ss->setPlanner(planner);

    builder.setup();

    goal->startSampling();
    ompl::base::PlannerStatus solved = builder.ss->solve(200.0);
    goal->stopSampling();

    OMPL_DEBUG("HAVE EXACT SOLUTION PATH: ");
    std::cout << builder.ss->haveExactSolutionPath() << std::endl;

    if (solved == ompl::base::PlannerStatus::EXACT_SOLUTION )
    {
        ompl::geometric::PathGeometric path(builder.getSolutionPath(true,false));
        builder.ss->getPathSimplifier()->simplifyMax(path);
//        path.interpolate();
//        builder.info->getStateSpace()->copyToReals(solution_config,path.getStates().back());
        for(auto st : path.getStates()){
            ompl::base::State * st_ = builder.info->allocState();
            builder.space->copyState(st_,st);
            solution_states.push_back(st_);
        }
//        solution_states = path.getStates();
        return true;
    }
    else{
        RBX_WARN("No solution found");
        return false;
    }

}

bool plan_to_move (std::shared_ptr<darts::World> &world,darts::Window &window,Object &obj, ActionR action_,
                            std::shared_ptr<darts::Robot> &robot_, std::shared_ptr<darts::Robot> &obj_robot_, int surface_no ) {

    darts::PlanBuilder builder(world);

    builder.addGroup(robot_->getName(), GROUP_X);
    builder.addGroup(obj_robot_->getName(), obj.group_name);

    builder.setStartConfigurationFromWorld();
    auto idk = builder.getStartConfiguration();
    Eigen::VectorXd backup_state(int(world->getRobot(obj_robot_->getName())->getGroupJoints(obj.group_name).size()));
    Eigen::VectorXd backup_state_fetch(int(world->getRobot(robot_->getName())->getGroupJoints(GROUP_X).size()));
    world->getRobot(obj_robot_->getName())->getGroupState(obj.group_name,backup_state);
    world->getRobot(robot_->getName())->getGroupState(GROUP_X,backup_state_fetch);

    add_constraint_to_builder(world,builder,obj,robot_,obj_robot_);

    builder.initialize();

    darts::TSR::Specification pos_spec;
    pos_spec.setFrame(robot_->getName(),"gripper_link", "move_x_axis");
    pos_spec.setPoseFromWorld(world);
    auto rotation = pos_spec.getRotation();
    Eigen::Vector3d position = pos_spec.getPosition();

    std::cout << position << std::endl;
    std::cout << rotation.w() << std::endl;

    translateActionToGoalRegion(position,rotation,action_,obj,surface_no);


    /*
//    std::vector<std::vector<ompl::base::State *>> solutions;

    ompl::geometric::PathGeometric path_(builder.info);  // TO APPY PATH HYBRIDIZATION
    int i = 0;
    while (i < 10){
        std::vector<ompl::base::State *> solution_states;
        if(!plan_goal_vector(robot_,obj_robot_,idk,obj,action_,surface_no,solution_states)){
            world->getRobot(obj_robot_->getName())->setGroupState(obj.group_name,backup_state);
            world->getRobot(robot_->getName())->setGroupState(GROUP_X,backup_state_fetch);
            i++;
        }else{

            world->getRobot(obj_robot_->getName())->setGroupState(obj.group_name,backup_state);
            world->getRobot(robot_->getName())->setGroupState(GROUP_X,backup_state_fetch);
            solutions.push_back(solution_states);
            i++;
        }
    }
     */
//    auto goal = builder.getGoalConfiguration(solution_config);

    auto goal = builder.getGoalTSR(create_goalTSR(eigen_quaternion_to_matrix(rotation),position,robot_->getName(),world));

    goal->setThreshold(0.001);
    builder.setGoal(goal);

    auto planner = std::make_shared<ompl::geometric::KPIECE1>(builder.info);
    builder.ss->setPlanner(planner);

    builder.setup();

    ompl::base::PlannerStatus solved = builder.ss->solve(30.0);

    OMPL_DEBUG("HAVE EXACT SOLUTION PATH: ");
    std::cout << builder.ss->haveExactSolutionPath() << std::endl;


    if (solved == ompl::base::PlannerStatus::EXACT_SOLUTION )
    {
        ompl::geometric::PathGeometric path(builder.getSolutionPath(true,false));

        if(path.getStateCount() > 4 && builder.info->getStateDimension() ==12 ){

            world->getRobot(obj_robot_->getName())->setGroupState(obj.group_name,backup_state);
            world->getRobot(robot_->getName())->setGroupState(GROUP_X,backup_state_fetch);
            return false;
        }
        else
            path.interpolate();

        window.animatePath(builder, path);
        obj.actual_position += action_.pos;
        obj.actual_rotation += action_.rpy;

        return true;
    }
    else{
        RBX_WARN("No solution found");
        world->getRobot(obj_robot_->getName())->setGroupState(obj.group_name,backup_state); // TODO
        world->getRobot(robot_->getName())->setGroupState(GROUP_X,backup_state_fetch); // TODO

        return false;
    }
}

bool plan_to_move_robot (std::shared_ptr<darts::World> &world,darts::Window &window,Vector3d new_position) {
    darts::PlanBuilder builder(world);
    builder.addGroup("fetch", "move_robot");
    builder.setStartConfigurationFromWorld();
    auto start_config = builder.getStartConfiguration();
    builder.initialize();

    std::vector<double> goal_config = {new_position[0],new_position[1],0.0 }; // y axis first, x axis second
    auto goal = builder.getGoalConfiguration(goal_config);
    builder.setGoal(goal);
    auto rrt = std::make_shared<ompl::geometric::RRTConnect>(builder.info,true);
    builder.ss->setPlanner(rrt);
    builder.setup();
    ompl::base::PlannerStatus solved = builder.ss->solve(20.0);


    if (solved == ompl::base::PlannerStatus::EXACT_SOLUTION )
    {
        RBX_INFO("Found solution!");
        while(true)
            window.animatePath(builder, builder.getSolutionPath());

    }
    else{
        RBX_WARN("No solution found");
        return false;
    }
}
