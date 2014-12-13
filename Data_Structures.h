/*
Homework II, Operating Systems
Fall, 2014
William Rory Kronmiller
RIN: 661033063
*/

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H
#include <stdlib.h>
#include <list>

//Process type enum
enum proc_type
{
	p_int, //Interactive process
	p_cpu //CPU-bound process
};

//Process Struct
struct Process
{
	unsigned int PID; //Process ID

	proc_type type; //CPU-bound or user-interactive process

	int cpu_cycles_left = 0; //CPU Cycles required for current operation
	int io_cycles_left = 0; //Cycles remaining before IO operation completed
	int cycles_completed = 0; //Cycles spent in current mode
	int priority = -1; //Priority value for Preemptive Priority algorithm
	int wait_cycles_left = 0; //Cycles remaining for context switch queue
	
	int bursts_left = -1; //Number of bursts remaining, before process should terminate
	int num_context_switches = 0; //Number of context switches performed

	long int time_entering_queue; //Wall clock time of queue entry

	//History lists
	std::list<int>
		cpu_times, //List of times spent in CPU
		queue_times, //List of times spent in queue (wait time history)
		turnaround_times; //Turnaround-time history

};

//CPU Struct
struct CPU
{
	unsigned int ID; 

	Process * loaded_process = NULL; //Process currently being run, NULL if idle

	int cycles_to_switch = -1; //Number of cycles remaining until current process should be unloaded (-1 if idle)
	/*
	cycles_to_switch should be set by manager either to process's cpu_cycles_left or timeslice time, whichever is smaller;
	preempting bool should be set accordingly
	*/

	bool preempting = false; //Current cycles_to_switch will preempt process

	int num_context_switches = 0; //Number of context switches performed

	long int active_cycles = 0; //Total number of cycles spent running processes
	long int idle_cycles = 0; //Total number of cycles spent idle
};

//List of scheduling algorithms
enum scheduling_algorithms
{
	SJF_nopreempt, //Shortest job first
	SJF_preempt,
	RR_preempt, //Round-robin
	PrePri //Preemptive priority
};

//Tunable run-time settings struct
struct system_settings
{
	int num_procs = 12; //Number of processes
	int num_CPUs = 4; //Number of CPUs

	int percent_interactive = 80; //Percentage of processes that should be interactive
	int interactive_burst_min = 200; //Minimum ms of interactive process burst time
	int interactive_burst_max = 3000; //Maximum ms of interactive process burst time
	int io_wait_min = 1200; //Minimum ms of IO wait time
	int io_wait_max = 3200; //Maximum ms of IO wait time

	int pcpu_burst_min = 50; //Minimum ms of cpu-bound process burst time
	int pcpu_burst_max = 300; //Maxmimum ms of cpu-bound process burst time
	int pcpu_burst_count = 8; //Number of bursts to complete before termination

	int context_switch_delay = 2; //Number of ms to wait for context-switch

	//Scheduling-algorithm-specific Settings
	int rr_timeslice = 100; //Length of time-slice for Round-Robin
	int pp_num_priorities = 4; //Number of PP priority levels
	int pp_priority_increment = 1200; //Number of ms before priority increase

};


#endif //DATA_STRUCTURES_H