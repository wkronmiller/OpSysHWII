/*
Homework II, Operating Systems
Fall, 2014
William Rory Kronmiller
RIN: 661033063
*/

#include <iostream>
#include <unistd.h>
#include "ProcessManager.h"
#include "Data_Structures.h"

#define TEST_SJF 1
#define TEST_SJF_PREEMPT 1
#define TEST_RR 1
#define TEST_PRE_PRI 1
#define NUM_TESTS 1

/*Note to TA's: This program has to be compiled with C++11

On another note, the tunable settings are in Data_Structures.h in the system_settings struct.*/

//Run single manager
int run_manager(ProcessManager& pm)
{
	pm.init();

	bool running;

	do
	{
		try
		{
			running = pm.tick();
		}
		catch (PM_Exception& e)
		{
			return EXIT_FAILURE;
		}
		catch (...)
		{
			std::cerr << "ERROR: Unhandled exception" << std::endl;
			return EXIT_FAILURE;
		}

	} while (running);

	pm.print_results();

	return EXIT_SUCCESS;
}

//Execute simulations of all methods
int simulate_all_methods()
{
	int return_val = EXIT_SUCCESS;
	system_settings default_settings;

#if TEST_SJF

	//Test Shortest Job First w/out Preempt
	std::cout << "Creating SJF ProcessManager..." << std::endl;
	ProcessManager sjf_manager(SJF_nopreempt, default_settings);
	return_val = return_val | run_manager(sjf_manager);

#endif

#if TEST_SJF_PREEMPT

	//Test Shortest Job First w/Preempt
	std::cout << "Creating SJF Preemptive ProcessManager..." << std::endl;
	ProcessManager sjf_p_manager(SJF_preempt, default_settings);
	return_val = return_val | run_manager(sjf_p_manager);

#endif

#if TEST_RR

	//Test Round-Robin
	std::cout << "Creating Round Robin ProcessManager..." << std::endl;
	ProcessManager rr_manager(RR_preempt, default_settings);
	return_val = return_val | run_manager(rr_manager);

#endif

#if TEST_PRE_PRI

	//Test Preemptive Priority
	std::cout << "Creating Preemptive Priority ProcessManager..." << std::endl;
	ProcessManager pp_manager(PrePri, default_settings);
	return_val = return_val | run_manager(pp_manager);

#endif

	//Return results
	return return_val;
}

//Main function
int main(int argc, char * argv[])
{
	for (int test_iteration = 0; test_iteration < NUM_TESTS; test_iteration++)
	{
		if (simulate_all_methods() != EXIT_SUCCESS)
		{
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}