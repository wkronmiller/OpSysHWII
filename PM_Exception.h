#pragma once

#include <iostream>
#include <exception>
#include <stdlib.h>
#include <string>

//Custom exception class
class PM_Exception : public std::exception
{
public:
	//Default constructor
	PM_Exception()
	{
		this->err_msg = "Generic Error";
	}

	//Constructor with specified error
	PM_Exception(const std::string& er_msg)
	{
		this->err_msg = er_msg;
	}

	virtual const char* what() const throw()
	{
		return err_msg.c_str();
	}

private:
	std::string err_msg;
};

//Custom exception function
void pm_throw(const std::string& error_message);