#include "PM_Exception.h"

//Custom exception-throwing function
void pm_throw(const std::string& error_message)
{
	PM_Exception custom_except(error_message);

	//Print error message to console
	std::cerr << error_message << std::endl;

	//Throw error
	throw custom_except;
}

