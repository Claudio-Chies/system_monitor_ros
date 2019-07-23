/*
 * system_monitor_node.cpp
 *
 *  Created on: Dez 05, 2018
 *      Author: Tanja Baumann
 *
 *      launch example:
 *      roslaunch system_monitor_ros n_processes:=10
 */

#include <ros/ros.h>
#include <iostream>
#include <fstream>
#include <std_msgs/String.h>
#include <mavros_msgs/StatusText.h>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>


std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

void sendWarningCPU() {

}


int main(int argc, char* argv[])
{
	ros::init(argc, argv, "system_monitor_node");
	ros::NodeHandle nh("~");

	//get parameter
	int n_processes;
	nh.param<int>("n_processes", n_processes, 8);

	//initialize publishers
	ros::Publisher cpu_pub_;
	ros::Publisher memory_pub_;
	ros::Publisher processes_pub_;
	ros::Publisher mavros_cpu_warning_pub_;
	ros::Time t_cpu_sent = ros::Time::now();

	cpu_pub_ = nh.advertise<std_msgs::String>("/cpu_usage", 1);
	memory_pub_ = nh.advertise<std_msgs::String>("/memory_usage", 1);
	processes_pub_ = nh.advertise<std_msgs::String>("/processes", 1);
	mavros_cpu_warning_pub_ = nh.advertise<mavros_msgs::StatusText>("/mavros/statustext/send", 1);


	while (ros::ok()) {

	    // define messages
	    std_msgs::String cpu_msg;
	    std_msgs::String mem_msg;
	    std_msgs::String proc_msg;

	    //get cpu and memory usage
	    std::string cpu = exec(" top -bn 1 | sed -n 3p");
	    std::string mem = exec(" top -bn 1 | sed -n 4p");

	    //Get n_processes most CPU-hungry processes
	    std::string command = "ps -e -o pid,pcpu,pmem,args --sort=-pcpu |  head -n ";
	    command.append(std::to_string(n_processes + 1));
	    command.append(" | cut -d' ' -f1-8");
	    std::string processes = exec(command.c_str());

	    cpu_msg.data = cpu;
	    mem_msg.data = mem;
	    proc_msg.data = processes;

	    //publish system data
	    cpu_pub_.publish(cpu_msg);
	    memory_pub_.publish(mem_msg);
	    processes_pub_.publish(proc_msg);

	    //get idle CPU for evaluation
	    std::stringstream ss(cpu);
	    std::vector<std::string> cpu_data;

	    while( ss.good() )
	    {
	        std::string substr;
	        getline( ss, substr, ',' );
	        cpu_data.push_back( substr );
	    }

	    std::string cpu_idle_part = cpu_data[3];
	    std::string cpu_idle_string = cpu_idle_part.substr (0,cpu_idle_part.size()-2);

	    //send warning if less than 20% CPU is idle
	    if(std::stof(cpu_idle_string) < 20.0 && ros::Time::now() - t_cpu_sent > ros::Duration(5.0)){
	    	mavros_msgs::StatusText msg;
	        msg.header.stamp = ros::Time::now();
	        msg.severity = 2; //CRITICAL
	        msg.text = ("CPU warning, only " + cpu_idle_string + "% idle").c_str();
	        mavros_cpu_warning_pub_.publish(msg);
	        t_cpu_sent = ros::Time::now();
	    }


		ros::Duration(1).sleep();
	}


    return 0;
}
