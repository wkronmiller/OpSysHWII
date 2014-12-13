/*
Homework II, Operating Systems
Fall, 2014
William Rory Kronmiller
RIN: 661033063
*/

#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "Data_Structures.h"
#include "PM_Exception.h"
#include <iostream>
#include <exception>
#include <stdlib.h>
#include <vector>
#include <map>

//Process Manager Class/Function Headers
class ProcessManager
{
public:

	//Constructors
	ProcessManager();
	ProcessManager(const scheduling_algorithms algorithm, const system_settings settings);

	//Execution functions
	int init(); //Enqueues all processes
	bool tick(); //Simulates clock tick, returns true when done

	//Log Functions
	void log_context_switch(const int PID_out, const int PID_in);
	void log_process_entry(const int PID);
	void log_process_cpu_burst(const int PID);
	void log_process_termination(const int PID);
	void log_aging_event(const int PID);
	void log_analysis(); //Prints simulation analysis

	//Diagnostics
	void dump_ready_queue();
	void print_results();

	//Destructor
	~ProcessManager();

private:

	//Helper functions
	bool swap_procs(); //Swap in new process to CPU
	void free_procs(); //Remove processes from CPUs when bursts are finished
	void tick_CPUs(); //Increment CPU counters
	void dispatch_cpus(); //CPU context-switch operations
	void dispatch_io(); //IO start/completion operations
	void check_aging(); //Generate aging events
	void print_log(const std::string& message);
	int find_free_cpu(const int preempt_priority, const int preempt_time); //Find free cpu, return -1 if none free
	int find_next_process(); //Find next process, using selected algorithm, return -1 if none ready
	void load_process(const unsigned int PID_to_CPU, const unsigned int CPU_ID); //Load process into CPU, removing existing process if necessary
	void unload_process(int CPU_ID); //Unload process from CPU
	void enqueue_process(const unsigned int PID); //Add a process to the ready queue
	void unqueue_process(const unsigned int PID); //Remove a process from the ready queue

	//Algorithms -- return index value of next process to load into CPU
	int SJF();
	int RR();
	int PP();

	//CPUs
	std::vector<CPU> cpus;
	
	//Processes
	std::vector<Process> master_proc_list;

	//Queues
	std::list<Process *> ready_queue; //List of processes waiting for CPU
	std::list<Process *> context_queue_leaving_cpu; //List of processes in context-switch out of CPU
	std::vector<Process *> context_queue_entering_cpu; //Vector of processes context-switching into CPU (index value is CPU ID)
	std::list<Process *> io_queue; //List of processes doing io

	//Wall clock time
	long int wall_clock = 0; //Incremented by tick() function
	int running_cpu_bound = 0; //Number of running cpu_bound processes

	//Settings
	system_settings runtime_settings; //Tunable settings
	scheduling_algorithms algorithm;
	
};

#endif //PROCESS_MANAGER_H
