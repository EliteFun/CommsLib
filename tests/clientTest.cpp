#include <Client.hpp>

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

//uint16_t get_port_from_file(const std::string& filePath)
//{
//	std::ifstream file;
//	file.open(filePath);
//
//	std::string line;
//	std::getline(file, line);
//
//	file.close();
//
//	return std::stoi(line);
//}

int main()
{
	//{
	//	cl::Server commsServer(43215);
//
	//	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//	{
	//		cl::Client c(43215, 1, true);
	//		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//		c.update(1.0f);
	//		{
	//			cl::Client c2(43215, 2, false);
	//			c.update(0.1f);
	//			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//			c2.update(1.0f);
	//		}
	//	}
	//	
//
	//	std::cin.get();
	//}

	{
		cl::Client c(43215, 1, true);

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		cl::Client c2(43215, 2, false);

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		for (int i = 0; i < 50; i++)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			c.update(0.1f);
			c2.update(0.1f);
		}
	}
	
	std::cout << "Done." << std::endl;
	std::cin.get();

	return 0;
}
