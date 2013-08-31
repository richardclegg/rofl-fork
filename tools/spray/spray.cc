#include "cgetopt.h"
#include "cudpsend.h"
#include "cudprecv.h"

int
main(int argc, char** argv)
{
	unixenv::cgetopt& getopt = unixenv::cgetopt::get_instance();

	getopt.add_long_option("sender", 	getopt.NO_ARG);
	getopt.add_long_option("receiver", 	getopt.NO_ARG);
	getopt.add_long_option("local", 	getopt.REQUIRED_ARG);
	getopt.add_long_option("remote",  	getopt.REQUIRED_ARG);
	getopt.add_long_option("duration",	getopt.REQUIRED_ARG);

	getopt.parse(argc, argv);

	rofl::caddress local(AF_INET, "0.0.0.0", 5001);
	if (getopt.has_opt("local")) {
		std::string s_addr = getopt.get_opt("local").substr(0, getopt.get_opt("local").find_first_of(":"));
		std::string s_port = getopt.get_opt("local").substr(getopt.get_opt("local").find_first_of(":")+1);
		local = rofl::caddress(AF_INET, s_addr.c_str(), atoi(s_port.c_str()));
	}

	rofl::caddress remote(AF_INET, "0.0.0.0", 6001);
	if (getopt.has_opt("remote")) {
		std::string s_addr = getopt.get_opt("remote").substr(0, getopt.get_opt("remote").find_first_of(":"));
		std::string s_port = getopt.get_opt("remote").substr(getopt.get_opt("remote").find_first_of(":")+1);
		remote = rofl::caddress(AF_INET, s_addr.c_str(), atoi(s_port.c_str()));
	}

	std::cerr << "local: " << local.c_str() << std::endl;
	std::cerr << "remote: " << remote.c_str() << std::endl;

	int duration = 10;
	if (getopt.has_opt("duration")) {
		duration = atoi(getopt.get_opt("duration").c_str());
	}
	(void)duration;

	if (getopt.has_opt("sender")) {

		spray::cudpsend udpsend(remote, local);

		udpsend.start_sending();

		//sleep(duration);

		//udpsend.stop_sending();

		rofl::ciosrv::run();

	} else if (getopt.has_opt("receiver")) {

		spray::cudprecv udprecv(remote, local);

		udprecv.start_receiving();

		//sleep(10);

		//udprecv.stop_receiving();

		rofl::ciosrv::run();

	} else {
		std::cerr << "unknown mode, aborting." << std::endl;
		return -1;
	}

	return 0;
}
