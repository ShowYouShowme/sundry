#include "CPing.h"
#include <iostream>
#include <string>
#include <windows.h>
int main()
{
	auto p = CPing::getInstance();
	p->init("www.jd.com", 0);
	while (1)
	{
		
	}
	return 1;
}