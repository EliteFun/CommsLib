#include <Server.hpp>

#include <iostream>

int main()
{
	{
		cl::Server commsServer(43215);

		std::string input = "";
		while (commsServer.isRunning())
		{
			std::cin >> input;

			if (input == "stop")
			{
				break;
			}
			else if (input == "clients")
			{
				commsServer.printClients();
			}
			else if (input == "ping")
			{
				commsServer.pingClients();
			}
			else if (input == "dcall")
			{
				
			}
			else if (input == "help")
			{
				std::cout << "Available commands:\n\n"
					      << "\tstop       stops the server.\n"
					      << "\tclients    shows a list of connected clients.\n"
						  << "\tping       pings all clients\n"
						  << "\tdcall      disconnects all clients\n"
						  << "\thelp       shows this help message\n"
						  << std::endl;
			}
			else
			{
				// yeah, ik, same thing as help, but whatever, this file is called "test" for a reason after all
				std::cout << "Unknown command: " << input << ". Available commands:\n\n"
					      << "\tstop       stops the server.\n"
					      << "\tclients    shows a list of connected clients.\n"
						  << "\tping       pings all clients\n"
						  << "\tdcall      disconnects all clients\n"
						  << "\thelp       shows this help message\n"
						  << std::endl;
			}
		}
	}

	std::cout << "\nPress enter to quit..." << std::endl;
	std::cin.get(); // why?
	std::cin.get();

	return 0;
}
