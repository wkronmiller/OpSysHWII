/*
Homework II, Operating Systems
Fall, 2014
William Rory Kronmiller
RIN: 661033063
*/

#include "ProcessManager.h"
#include <time.h>
#include <sstream>

//Seed with entropy?
#define NORANDOM 0

//Default constructor
ProcessManager::ProcessManager()
{
	//Should never be called
	std::cerr << "ERROR: ProcessManager constructor given invalid input!" << std::endl;

	throw("Process Manager: Invalid constructor");
}

//Algorithm constructor
ProcessManager::ProcessManager(const scheduling_algorithms algorithm, const system_settings settings)
{
#ifdef NORANDOM

	//Initialize random seed with consistent value
	srand(100);

#else
	//Initialize random seed with changing value
	srand(time(NULL));

#endif

	//Copy in settings
	this->algorithm = algorithm;
	this->runtime_settings = settings;

	//Initialize processes
	int num_interactive = (settings.num_procs * settings.percent_interactive) / 100;
	int num_cpu_bound = settings.num_procs - num_interactive;

	int PID = 0;

	//Error-checking
	if (settings.pcpu_burst_count < 1)
	{
		pm_throw("ProcessManager: invalid CPU burst count");
		return;
	}

	//Create processes
	while (num_cpu_bound > 0 || num_interactive > 0)
	{
		//Add CPU process
		if (num_cpu_bound-- > 0)
		{
			//Initialize data
			Process new_cpu;
			new_cpu.type = p_cpu;
			new_cpu.PID = PID++;
			new_cpu.bursts_left = settings.pcpu_burst_count;
			new_cpu.priority = rand() % runtime_settings.pp_num_priorities;

			//Put in master list of all processes
			this->master_proc_list.push_back(new_cpu);

			//Update count of running CPU-bound processes
			this->running_cpu_bound++;
		}
		//Add interactive process
		if (num_interactive-- > 0)
		{
			//Initialize data
			Process new_interactive;
			new_interactive.type = p_int;
			new_interactive.PID = PID++;
			new_interactive.priority = rand() % runtime_settings.pp_num_priorities;

			//Put in master list of all processes
			this->master_proc_list.push_back(new_interactive);
		}
	}
	
	//Create CPUs
	for (int cpu_count = 0; cpu_count < settings.num_CPUs; cpu_count++)
	{
		CPU new_cpu;
		new_cpu.ID = cpu_count;
		this->cpus.push_back(new_cpu);

		//Initialize context entry queue
		this->context_queue_entering_cpu.push_back(NULL); 
	}

}

//Enqueue all processes, generating metadata where necessary
int ProcessManager::init()
{
	//Add all processes to ready queue
	for (std::vector<Process>::const_iterator itr = master_proc_list.begin(); itr < master_proc_list.end(); itr++)
	{
		this->enqueue_process(itr->PID);
	}

	return EXIT_SUCCESS;
}

//Member helper function to check ready queue aging
void ProcessManager::check_aging()
{
	unsigned int age_time = this->runtime_settings.pp_priority_increment;
	int max_priority = this->runtime_settings.pp_num_priorities;

	//Error-checking
	if (this->algorithm != PrePri)
	{
		pm_throw("ProcessManager: attempting to implement aging with invalid algorithm");
		return;
	}

	for (std::list<Process *>::iterator itr = this->ready_queue.begin(); itr != this->ready_queue.end(); itr++)
	{
		Process * proc = (*itr);

		//Check if process needs to be aged
		if ((proc->cycles_completed > 1) && (proc->cycles_completed % age_time == 0) &&(proc->priority < max_priority))
		{
			//Log aging
			this->log_aging_event(proc->PID);

			//Increase priority
			proc->priority++;
		}
	}
}

//Member helper function to dispatch IO queue
void ProcessManager::dispatch_io()
{
	for (std::list<Process *>::iterator itr = this->io_queue.begin(); itr != this->io_queue.end();)
	{
		Process * proc = (*itr);

		//Check if process is done with IO
		if (proc->io_cycles_left == 0)
		{
			//Update metrics
			proc->cycles_completed = 0;

			//Add to ready queue
			enqueue_process(proc->PID);

			//Remove from IO list
			itr = this->io_queue.erase(itr);
		}
		else
		{
			//Increment iterator
			itr++;
		}
	}
}

//Non-Member Helper Function to Tick IO Queue
void tick_IO(std::list<Process *>& io_queue)
{
	for (std::list<Process *>::iterator itr = io_queue.begin(); itr != io_queue.end(); itr++)
	{
		Process * proc = (*itr);

		//Error-checking
		if (proc->io_cycles_left < 0)
		{
			pm_throw("IO Queue: Process stuck in queue");
			continue;
		}
		else if (proc->type == p_cpu)
		{
			pm_throw("IO Queue: CPU-bound process is attempting IO operation");
			continue;
		}

		//Update metrics
		proc->cycles_completed++;
		proc->io_cycles_left--;
	}
}

//Member helper function to tick all CPUs
void ProcessManager::tick_CPUs()
{
	for (unsigned int cpu_id = 0; cpu_id < cpus.size(); cpu_id++)
	{
		Process * curr_process = cpus[cpu_id].loaded_process;

		if (curr_process == NULL && (this->context_queue_entering_cpu.at(cpu_id) == NULL)) //CPU is idle
		{
			cpus[cpu_id].idle_cycles++;
		}
		else if (curr_process != NULL)
		{
			//Update process metrics
			curr_process->cpu_cycles_left--;
			curr_process->cycles_completed++;
			
			//Update CPU metrics
			cpus[cpu_id].active_cycles++;
			cpus[cpu_id].cycles_to_switch--;
		}
	}
}

//Non-member helper function to load process into CPU
void load_proc_cpu(Process * process, CPU * processor)
{
	//Error-checking
	if (processor->loaded_process != NULL)
	{
		pm_throw("DISPATCHER: CPU should not have loaded process");
		return;
	}
	if (processor->cycles_to_switch != process->cpu_cycles_left && !(processor->preempting))
	{
		pm_throw("ProcessManager: CPU has invalid cycle count");
		return;
	}

	//Load process
	processor->loaded_process = process;

	//Set process metrics
	process->cycles_completed = 0;
}

//Dispatch processes to CPUs and to IO
void ProcessManager::dispatch_cpus()
{
	//Context queue entering CPU
	for (unsigned int CPU_ID = 0; CPU_ID < this->context_queue_entering_cpu.size(); CPU_ID++)
	{
		Process * proc = this->context_queue_entering_cpu.at(CPU_ID);

		if (proc != NULL)
		{
			if (proc->wait_cycles_left == 0)
			{
				//Load process into CPU
				load_proc_cpu(proc, &(this->cpus.at(CPU_ID)));

				//Remove from context-switch vector
				this->context_queue_entering_cpu.at(CPU_ID) = NULL;

				//Update process's metrics
				proc->num_context_switches++;
			}
		}
	}

	//Context queue leaving CPU
	for (std::list<Process *>::iterator itr = this->context_queue_leaving_cpu.begin(); itr != this->context_queue_leaving_cpu.end();)
	{
		if ((*itr)->wait_cycles_left == 0)
		{
			//Update metrics
			(*itr)->num_context_switches++;

			//Determine whether to send to IO or back to ready queue
			if ((*itr)->cpu_cycles_left != 0 || (*itr)->type == p_cpu)
			{
				//Reset IO Metrics
				(*itr)->io_cycles_left = 0;

				//Send to ready queue
				enqueue_process((*itr)->PID);
			}
			else
			{
				//Log burst completion
				log_process_cpu_burst((*itr)->PID);

				//Error-checking
				if ((((*itr)->io_cycles_left < 1)&&((*itr)->type == p_int))&&(this->runtime_settings.io_wait_min >= 1))
				{
					pm_throw("ProcessManager: Invalid io wait time");
					continue;
				}

				//Update Metrics
				Process * proc = (*itr);
				proc->cycles_completed = 0;
				
				//Enqueue Process
				this->io_queue.push_back(proc);
			}

			//Remove process from wait queue
			itr = this->context_queue_leaving_cpu.erase(itr);
		}
		else
		{
			//Increment iterator
			itr++;
		}
	}
}

//Non-member helper function to tick all waiting processes
void tick_ready_queue(std::list<Process *>& ready_queue)
{
	//Iterate over list
	for (std::list<Process *>::iterator itr = ready_queue.begin(); itr != ready_queue.end(); itr++)
	{
		(*itr)->cycles_completed++;
	}
}

//Non-member helper function to tick all context-switching processes
void tick_context_queues(std::vector<Process *>& enter_queue, std::list<Process *>& leave_queue)
{
	//Tick enter queue
	for (std::vector<Process *>::iterator itr = enter_queue.begin(); itr != enter_queue.end(); itr++)
	{
		//Skip empty queue segments
		if ((*itr) == NULL)
		{
			continue;
		}

		(*itr)->cycles_completed++;
		(*itr)->wait_cycles_left--;

		//Error-checking
		if ((*itr)->wait_cycles_left < 0)
		{
			pm_throw("DISPATCHER: process stuck in context switch in list");
		}
	}

	//Tick exit queue
	for (std::list<Process *>::iterator itr = leave_queue.begin(); itr != leave_queue.end(); itr++)
	{
		(*itr)->cycles_completed++;
		(*itr)->wait_cycles_left--;

		//Error-checking
		if ((*itr)->wait_cycles_left < 0)
		{
			pm_throw("DISPATCHER: process stuck in context switch out list");
		}
	}
}

//Swap processes in CPU for processes in ready queue
bool ProcessManager::swap_procs()
{
	//Check for waiting process
	int PID = find_next_process();

	if (PID == -1)
	{
		//No waiting processes
		return false;
	}

	//Get pointer to process
	Process * proc = &(this->master_proc_list.at(PID));

	//Error-checking
	if (proc == NULL)
	{
		pm_throw("ProcessManager: process not in ready queue");
		return false;
	}

	//Set preemption values based on active algorithm
	int preempt_priority = -1, preempt_time = -1;

	if (this->algorithm == PrePri)
	{
		preempt_priority = proc->priority;
	}
	else if (this->algorithm == SJF_preempt)
	{
		preempt_time = proc->cpu_cycles_left;
	}

	//Check for free CPU
	int cpu_id = this->find_free_cpu(preempt_priority, preempt_time);

	if (cpu_id == -1)
	{
		//No free CPUs
		return false;
	}

	//Initiate context switch
	load_process(PID, cpu_id);

	return true;
}

//Free processes that are ready to terminate
void ProcessManager::free_procs()
{
	//Check for CPUs containing finished processes
	for (unsigned int idx = 0; idx < this->cpus.size(); idx++)
	{
		if ((this->cpus.at(idx).cycles_to_switch == 0) && (this->cpus.at(idx).loaded_process != NULL))
		{
			this->unload_process(this->cpus.at(idx).ID);
		}
		else if (this->cpus.at(idx).cycles_to_switch < 0) //Error-checking
		{
			pm_throw("ProcessManager: CPU has invalid cycle count");
		}
	}
}

//Simulates clock tick
bool ProcessManager::tick()
{
	//Infinite-Loop Check
	if (this->wall_clock == __LONG_MAX__)
	{
		pm_throw("ProcessManager: Infinite loop suspected");
		return false;
	}

	//Check aging, if necessary
	if (this->algorithm == PrePri)
	{
		check_aging();
	}

	//Check for required context changes
	while (swap_procs()){} //Try to swap in new processes
	this->free_procs(); //Remove processes from CPU if burst completed, even if ready queue empty
	this->dispatch_cpus();
	this->dispatch_io();

	//Scroll through lists, updating counters

	//Update wall clock
	this->wall_clock++;

	//Update context-switch wait counters
	tick_context_queues(this->context_queue_entering_cpu, this->context_queue_leaving_cpu);

	//Update CPU and loaded process counters
	tick_CPUs();

	//Update IO queue
	tick_IO(this->io_queue);

	//Update ready queue
	tick_ready_queue(this->ready_queue);

	//Check for running cpu-bound processes
	if (this->running_cpu_bound > 0)
	{
		return true;
	}
	
	//No more running cpu-bound processes
	return false;
}

//Main logging function
void ProcessManager::print_log(const std::string& message)
{
	std::cout << "[time " << this->wall_clock << "ms] " << message << std::endl;
}

//Log context switch event
void ProcessManager::log_context_switch(const int PID_out, const int PID_in)
{
	std::stringstream message;

	message << "Context switch (swapping out process ID "
		<< PID_out;

	if (PID_in != -1)
	{
		message << " for process ID "
			<< PID_in;
	}

	message << ")";

	this->print_log(message.str());
}

//Log process entering ready queue
void ProcessManager::log_process_entry(const int PID)
{
	std::stringstream message;
	Process proc = this->master_proc_list[PID];

	if (proc.type == p_cpu)
	{
		message << "CPU-bound process";
	}
	else
	{
		message << "Interactive process";
	}

	message << " ID " << PID << " entered ready queue (requires " << proc.cpu_cycles_left << "ms CPU time; priority " << proc.priority << ")";

	this->print_log(message.str());

}

//Log Completion of CPU Burst
void ProcessManager::log_process_cpu_burst(const int PID)
{
	std::stringstream message;
	Process * proc = &(this->master_proc_list.at(PID));

	if (proc->type == p_cpu)
	{
		message << "CPU-bound process";
	}
	else
	{
		message << "Interactive process";
	}

	//Calculate latest turnaround time
	int turnaround_time = this->wall_clock - proc->time_entering_queue;

	//Store turnaround time
	proc->turnaround_times.push_back(turnaround_time);

	message << " ID "
		<< PID
		<< " CPU burst done (turnaround time "
		<< turnaround_time
		<< "ms, total wait time "
		<< proc->queue_times.back()
		<< "ms)";

	this->print_log(message.str());
}

//Helper function to find average value of contents of integer list
int int_list_avg(const std::list<int>& list)
{
	int sum = 0;

	//Sum all values
	for (std::list<int>::const_iterator itr = list.begin(); itr != list.end(); itr++)
	{
		sum += *itr;
	}

	//Calculate average
	sum /= list.size();

	return sum;
}

//Log termination of process
void ProcessManager::log_process_termination(const int PID)
{
	std::stringstream message;
	Process * proc = &(this->master_proc_list.at(PID));

	if (proc->type == p_cpu)
	{
		message << "CPU-bound process";
	}
	else
	{
		message << "Interactive process";
	}

	//Calculate latest turnaround time
	int turnaround_time = this->wall_clock - proc->time_entering_queue;

	//Store turnaround time
	proc->turnaround_times.push_back(turnaround_time);

	message << " ID "
		<< PID
		<< " terminated ( avg turnaround time "
		<< int_list_avg(proc->turnaround_times)
		<< "ms, avg total wait time "
		<< int_list_avg(proc->queue_times)
		<< "ms)";

	this->print_log(message.str());
}

//Log process aging event
void ProcessManager::log_aging_event(const int PID)
{
	std::stringstream message;
	Process proc = this->master_proc_list[PID];

	message << "Increased priority of ";

	if (proc.type == p_cpu)
	{
		message << "CPU-bound process";
	}
	else
	{
		message << "Interactive process";
	}

	message << " ID " 
		<< PID 
		<< " to " 
		<< proc.priority 
		<< " due to aging";

	this->print_log(message.str());
}

//Diagnostics: Print Contents of Ready Queue
void ProcessManager::dump_ready_queue()
{
	for (std::list<Process *>::const_iterator itr = this->ready_queue.begin(); itr != this->ready_queue.end(); itr++)
	{
		std::cout << (*itr)->PID << ": entered at time " << (*itr)->time_entering_queue << std::endl;
	}
}

//Non-Member Helper function to find a free CPU without preempting
int find_free_cpu_no_pe(const std::vector<CPU>& cpus, const std::vector<Process *>& waiting)
{
	//Search through CPU list
	for (unsigned int CPU_id = 0; CPU_id < cpus.size(); CPU_id++)
	{
		CPU curr_cpu = cpus.at(CPU_id);

		//Check whether CPU is ready to switch processes and whether there is a process waiting to enter
		if ((curr_cpu.cycles_to_switch < 1)&& (waiting[CPU_id] == NULL))
		{
			return CPU_id;
		}
	}

	return -1;
}

//Non-Member Helper function to find a free CPU with preempting by time
int find_free_cpu_pe_tim(const std::vector<CPU>& cpus, const std::vector<Process *>& waiting, const int preempt_time)
{
	std::map<int, int> preemptable_CPUs;

	//Search through CPU list
	for (unsigned int CPU_id = 0; CPU_id < cpus.size(); CPU_id++)
	{
		CPU curr_cpu = cpus.at(CPU_id);
		//Check whether CPU is running preemptable process and whether there is a process waiting to enter
		if ((waiting[CPU_id] == NULL) && (curr_cpu.cycles_to_switch > preempt_time))
		{
			preemptable_CPUs[curr_cpu.cycles_to_switch] = CPU_id;
		}
	}

	//Find available CPU with longest-running process (increase throughput)
	if (preemptable_CPUs.size() > 0)
	{
		int ret_val = (*(preemptable_CPUs.rbegin())).second;
		return ret_val;
	}

	//No CPU available
	return -1;
}

//Non-Member Helper function to find a free CPU with preempting by priority
int find_free_cpu_pe_pri(const std::vector<CPU>& cpus, const std::vector<Process *>& waiting, const int preempt_priority)
{
	std::map<int, int> preemptable_CPUs;

	//Search through CPU list
	for (unsigned int CPU_id = 0; CPU_id < cpus.size(); CPU_id++)
	{
		CPU curr_cpu = cpus.at(CPU_id);
		//Check whether CPU is running preemptable process and whether there is a process waiting to enter
		if ((waiting[CPU_id] == NULL) && (curr_cpu.loaded_process->priority < preempt_priority))
		{
			preemptable_CPUs[curr_cpu.cycles_to_switch] = CPU_id;
		}
	}

	//Find available CPU with longest-running process (increase throughput)
	if (preemptable_CPUs.size() > 0)
	{
		int ret_val = (*(preemptable_CPUs.rbegin())).second;
		return ret_val;
	}

	//No CPU available
	return -1;
}

//Find free CPU
int ProcessManager::find_free_cpu(const int preempt_priority, const int preempt_time)
{
	//Ensure CPU queue isn't empty
	if (this->cpus.size() < 1)
	{
		pm_throw("DISPATCHER: No CPUs in system");
		return EXIT_FAILURE;
	}

	//First, try to find a CPU that doesn't need preemption
	int no_pe_cpu = find_free_cpu_no_pe(this->cpus, this->context_queue_entering_cpu);
	if (no_pe_cpu != -1)
	{
		return no_pe_cpu;
	}
	else if (preempt_priority != -1) //Preempt by priority
	{
		return find_free_cpu_pe_pri(this->cpus, this->context_queue_entering_cpu, preempt_priority);
	}
	else if (preempt_time != -1) //Preempt by time
	{
		return find_free_cpu_pe_tim(this->cpus, this->context_queue_entering_cpu, preempt_time);
	}

	//No CPU available
	return -1;
}

//Find next process to load into CPU
int ProcessManager::find_next_process()
{
	//Ensure ready queue isn't empty
	if (this->ready_queue.size() < 1)
	{
		return -1;
	}

	//Shortest Job First Mode
	if (algorithm == SJF_nopreempt || algorithm == SJF_preempt)
	{
		return SJF();
	}
	else if (algorithm == RR_preempt) //Round Robin
	{
		return RR();
	}
	else if (algorithm == PrePri) //Preemptive Priority
	{
		return PP();
	}

	//General failure
	pm_throw("ProcessManager: attempting to use an unknown algorithm");

	return EXIT_FAILURE;
}

//Load process into CPU context queue, removing existing process if necessary
void ProcessManager::load_process(const unsigned int PID_to_CPU, const unsigned int CPU_ID)
{
	//Get process to add to CPU
	std::list<Process *>::iterator itr;
	for (itr = this->ready_queue.begin(); itr != this->ready_queue.end(); itr++)
	{
		if ((*itr)->PID == PID_to_CPU)
		{
			break;
		}
	}

	//Error-checking
	if (PID_to_CPU != (*itr)->PID)
	{
		pm_throw("DISPATCHER: process not in ready queue");
		return;
	}

	//Create pointers
	Process * p_In = (*itr);
	CPU * processor = &(this->cpus.at(CPU_ID));


	//Check if we need to unload existing process
	int original_process = -1;
	if (processor->loaded_process != NULL)
	{
		original_process = processor->loaded_process->PID;
		this->unload_process(CPU_ID);
	}

	//Log event
	if (original_process != -1)
	{
		log_context_switch(original_process, p_In->PID);
	}

	//Remove process from ready queue
	this->unqueue_process(PID_to_CPU);

	//Error-checking
	if (this->context_queue_entering_cpu.at(CPU_ID) != NULL)
	{
		pm_throw("Dispatcher: CPU already has waiting process");
	}

	//Add process to context-switching queue
	this->context_queue_entering_cpu.at(CPU_ID) = p_In;

	//Update process metrics
	p_In->wait_cycles_left = this->runtime_settings.context_switch_delay;

	//Set CPU timeslice if Round-Robin
	if ((this->algorithm == RR_preempt) && (this->runtime_settings.rr_timeslice < p_In->cpu_cycles_left))
	{
		processor->cycles_to_switch = this->runtime_settings.rr_timeslice;
		processor->preempting = true;
	}
	else
	{
		processor->cycles_to_switch = p_In->cpu_cycles_left;
		processor->preempting = false;
	}

}

//Unload process from CPU
void ProcessManager::unload_process(int CPU_ID)
{
	//Error-checking
	if (CPU_ID < 0 || CPU_ID > (int)this->cpus.size())
	{
		pm_throw("DISPATCHER: CPU ID not found");
	}

	//Get CPU
	CPU  * curr_cpu = &(this->cpus.at(CPU_ID));

	//Get Process
	Process * curr_proc = curr_cpu->loaded_process;

	//Error-checking
	if (curr_proc == NULL)
	{
		pm_throw("DISPATCHER: CPU has no process to unload");
	}

	//Send process to context-switch waiting area
	context_queue_leaving_cpu.push_back(curr_proc);

	//Set relevant process metrics
	curr_proc->cpu_times.push_back(curr_proc->cycles_completed);
	curr_proc->cycles_completed = 0;
	curr_proc->num_context_switches++;
	curr_proc->wait_cycles_left = (this->runtime_settings.context_switch_delay) / 2;

	//"Remove" process from CPU
	curr_cpu->loaded_process = NULL;
}

//Add process to ready queue
void ProcessManager::enqueue_process(const unsigned int PID)
{
	//Error-checking
	if (PID >= this->master_proc_list.size())
	{
		throw("READY_QUEUE: Attempting to load invalid PID");
	}

	for (std::list<Process *>::const_iterator itr = this->ready_queue.begin(); itr != this->ready_queue.end(); itr++)
	{
		//Check if process is already in list
		if ((*itr)->PID == PID)
		{
			throw("READY_QUEUE: Attempting to add process already in queue");
		}
	}

	//Load process
	Process * proc = &(master_proc_list.at(PID));

	//Check if process should terminate
	if ((proc->type == p_cpu) && (proc->bursts_left == 0) && (proc->cpu_cycles_left == 0))
	{
		//Log termination
		this->log_process_termination(PID);

		//Decrement active CPU-bound process count
		this->running_cpu_bound--;

		return;
	}

	//More error checking
	if ((proc->type == p_cpu) && (proc->bursts_left < 0))
	{
		pm_throw("DISPATCHER: CPU-bound process did not terminate on time");
	}

	//Generate burst time if necessary
	if (proc->cpu_cycles_left == 0)
	{
		int cpu_cycles_left;
		if (proc->type == p_cpu) //CPU-bound process
		{
			//Decrement burst count
			proc->bursts_left--;
			cpu_cycles_left = rand() % (runtime_settings.pcpu_burst_max - runtime_settings.pcpu_burst_min) + runtime_settings.pcpu_burst_min;
			
		}
		else //Interactive process
		{
			cpu_cycles_left = rand() % (runtime_settings.interactive_burst_max - runtime_settings.interactive_burst_min) + runtime_settings.interactive_burst_min;
		}

		proc->cpu_cycles_left = cpu_cycles_left;
	}
	else if (proc->cpu_cycles_left < 0)
	{
		pm_throw("ProcessManager: invalid cpu cycle count");
	}


	//Set metadata
	proc->time_entering_queue = this->wall_clock;
	proc->cycles_completed = 0;

	//Error-checking
	if (proc->wait_cycles_left + proc->io_cycles_left > 0)
	{
		pm_throw("READY_QUEUE: Process is not ready for ready queue");
	}


	//Add process to ready queue
	this->ready_queue.push_back(proc);

	//Log addition
	this->log_process_entry(PID);

	//Set IO Cycle count
	if (proc->type == p_int)
	{
		proc->io_cycles_left = rand() 
			% (runtime_settings.io_wait_max 
			- runtime_settings.pcpu_burst_min) 
			+ runtime_settings.pcpu_burst_min;
	}

}

//Remove process from ready queue
void ProcessManager::unqueue_process(const unsigned int PID)
{
	//Get process
	std::list<Process *>::iterator itr;

	for (itr = this->ready_queue.begin(); itr != this->ready_queue.end(); itr++)
	{
		if ((*itr)->PID == PID)
		{
			break;
		}
	}

	//Error-checking
	if (PID != (*itr)->PID)
	{
		pm_throw("READY_QUEUE: PID not in ready queue");
		return;
	}

	//Set pointer, for convenience
	Process * proc = (*itr);

	//Update process metrics
	proc->queue_times.push_back(proc->cycles_completed);
	proc->cycles_completed = 0;

	//Remove from ready queue
	itr = this->ready_queue.erase(itr);
}

//Algorithms=========================================================================================================
//Shortest-job-first (preemptive and non-preemptive)
int ProcessManager::SJF()
{
	//Ensure queue is not empty
	if (this->ready_queue.size() < 1)
	{
		return -1;
	}

	//Find shortest job
	std::list<Process *>::const_iterator itr = this->ready_queue.begin();
	int smallest_val = (*itr)->cpu_cycles_left;
	int sj_PID = (*itr)->PID;

	//Scroll through queue
	for (itr++; itr != this->ready_queue.end(); itr++)
	{
		int itr_val = (*itr)->cpu_cycles_left;
		int itr_PID = (*itr)->PID;

		//Check if itr's val is smaller than current minimum
		if (itr_val < smallest_val)
		{
			smallest_val = itr_val;
			sj_PID = itr_PID;
		}
	}

	//Return PID of shortest job
	return sj_PID;
}

//First come, first serve (non-member function)
int FCFS(const std::list<Process *>& queue)
{
	//Check for empty queue
	if (queue.size() < 1)
	{
		//Return error
		return -1;
	}

	//Get first element
	std::list<Process *>::const_iterator itr = queue.begin();

	//Return PID of first element
	return (*itr)->PID;
}

//Round-robin (Wrapper for FCFS)
int ProcessManager::RR()
{
	return FCFS(this->ready_queue);
}

//Preemptive Priority
int ProcessManager::PP()
{
	//Simulate multiple queues using single ready queue
	std::list<Process *>::const_iterator itr = this->ready_queue.begin();
	int highest_pri = (*itr)->cpu_cycles_left;
	int pp_PID = (*itr)->PID;

	//Scroll through queue
	for (itr++; itr != this->ready_queue.end(); itr++)
	{
		int itr_pri = (*itr)->priority;
		int itr_PID = (*itr)->PID;

		//Check if itr's val is smaller than current minimum
		if (itr_pri > highest_pri)
		{
			highest_pri = itr_pri;
			pp_PID = itr_PID;
		}
	}

	return pp_PID;
}

//Final analysis helper functions===============================
void turnaround_times(const std::vector<Process>& master_proc_list, std::stringstream& output)
{
	int min = -1,
		avg = 0,
		max = 0,
		cma_n = 0;

	//Iterate over list of processes
	for (std::vector<Process>::const_iterator proc_itr = master_proc_list.begin();
		proc_itr != master_proc_list.end(); proc_itr++)
	{
		//Iterate over each process's list of turnaround times
		for (std::list<int>::const_iterator time_itr = proc_itr->turnaround_times.begin(); 
			time_itr != proc_itr->turnaround_times.end(); time_itr++)
		{
			int current_val = (*time_itr);

			//Find minimum
			if (current_val < min || min == -1)
			{
				min = current_val;
			}

			//Calculate Cumulative Moving Average
			avg = ((current_val + (cma_n * avg)) / (cma_n + 1));
			cma_n++;

			//Find maximum
			if (current_val > max)
			{
				max = current_val;
			}
		}
	}

	//Generate text
	output << "Turnaround time: min "
		<< min << "ms; "
		<< "avg " << avg << "ms; "
		<< "max " << max << "ms"
		<< std::endl;
}

//Calculate wait time metrics
void wait_times(const std::vector<Process>& master_proc_list, std::stringstream& output)
{
	int min = -1,
		avg = 0,
		max = 0,
		cma_n = 0,
		local_total_wait;

	//Iterate over list of processes
	for (std::vector<Process>::const_iterator proc_itr = master_proc_list.begin();
		proc_itr != master_proc_list.end(); proc_itr++)
	{
		local_total_wait = 0;

		//Iterate over each process's list of wait times
		for (std::list<int>::const_iterator time_itr = proc_itr->queue_times.begin();
			time_itr != proc_itr->queue_times.end(); time_itr++)
		{
			local_total_wait += (*time_itr);
		}

		//Find minimum
		if (local_total_wait < min || min == -1)
		{
			min = local_total_wait;
		}

		//Calculate Cumulative Moving Average
		avg = ((local_total_wait + (cma_n * avg)) / (cma_n + 1));
		cma_n++;

		//Find maximum
		if (local_total_wait > max)
		{
			max = local_total_wait;
		}
	}

	//Generate text
	output << "Total wait time: "
		<< "min " << min << "ms; "
		<< "avg " << avg << "ms; "
		<< "max " << max << "ms"
		<< std::endl;
}

//Calculate average CPU utilization across all processors
void overall_average_cpu_utilization(const std::vector<CPU>& cpus, std::stringstream& output, const long int wall_clock)
{
	long long int cum_average = 0;

	for (std::vector<CPU>::const_iterator itr = cpus.begin(); itr != cpus.end(); itr++)
	{
		long int local_average =  ((long int)itr->active_cycles*100) / wall_clock;
		cum_average += (long long int)local_average;
	}

	cum_average /= (long long int)cpus.size();

	output << "Average CPU utilization: "
		<< cum_average
		<< "%"
		<< std::endl;
}

//Calculate average cpu utilization per process
void per_process_cpu_utilization(const std::vector<Process>& master_proc_list, std::stringstream& output, long int wall_clock)
{
	output << "Average CPU utilization per process: "
		<< std::endl;

	//Iterate over list of processes
	for (std::vector<Process>::const_iterator itr = master_proc_list.begin();
		itr != master_proc_list.end(); itr++)
	{
		long int local_total = 0;

		//Iterate over list of cpu times for each process
		for (std::list<int>::const_iterator times_itr = itr->cpu_times.begin();
			times_itr != itr->cpu_times.end(); times_itr++)
		{
			local_total += (long int)(*times_itr);
		}

		long int local_avg = (local_total * 100) / wall_clock;

		output << "process ID: " << itr->PID
			<< " " << local_avg << "%"
			<< std::endl;
	}
}

//Print final stats
void ProcessManager::print_results()
{
	//Error-checking
	if (this->master_proc_list.size() < 1)
	{
		pm_throw("ProcessManager: There are no processes to analyze");
		return;
	}
	if (this->cpus.size() < 1)
	{
		pm_throw("ProcessManager: There are no CPUs");
		return;
	}

	std::stringstream out_buffer;

	//Call helper functions
	turnaround_times(this->master_proc_list, out_buffer);
	wait_times(this->master_proc_list, out_buffer);
	overall_average_cpu_utilization(this->cpus, out_buffer, this->wall_clock);

	out_buffer << std::endl;

	per_process_cpu_utilization(this->master_proc_list, out_buffer, this->wall_clock);

	std::string results = out_buffer.str();

	std::cout << results;
}

//Destructor
ProcessManager::~ProcessManager()
{
}
