#include "ArgsParsing.hpp"

ProgramArgs::ProgramArgs() {
	error = "";
}

ProgramArgs::~ProgramArgs() {

}

void ProgramArgs::addIntParam(char shortName, const char* longName, int init) {
	struct arg arg = {
		shortName, longName, ARG_TYPE_INT
	};

	args.push_back(arg);
	result[longName] = init;
}

void ProgramArgs::addBoolParam(char shortName, const char* longName) {
	struct arg arg = {
		shortName, longName, ARG_TYPE_BOOL
	};
	
	args.push_back(arg);
	result[longName] = false;
}

int ProgramArgs::parseArgument(struct arg& argument, int argsAvail, char** argv) {
	switch (argument.type) {
	case ARG_TYPE_BOOL:
		result[argument.longName] = true;
		return 0;
	
	case ARG_TYPE_INT:
		if (argsAvail < 1) {
			error = "Missing parameter for arg " + std::string(argument.longName);
			return -1;
		}

		result[argument.longName] = atoi(argv[0]);
		return 1;
	default:
		error = "Unknown argument type for arg " + std::string(argument.longName);
		return -1;
	}
}

bool ProgramArgs::parse(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		char* arg = argv[i];
		char* argName;
		bool mustBeShort = false;
		
		if (arg[0] != '-' && arg[0] != '/') {
			error = "Unexpected positional argument";
			return false;
		}

		if (arg[1] == '-')
			argName = &arg[2];
		else
			argName = &arg[1];

		if (arg[0] == '-' && arg[1] != '-')
			mustBeShort = true;

		size_t nameLen = strlen(argName);
		if (nameLen < 1) {
			error = "Missing argument name";
			return false;
		}

		if (mustBeShort && nameLen > 1) {
			error = "Invalid name for short argument";
			return false;
		}

		if (nameLen == 1) {
			// Match by shortname
			for (auto& entry : args) {
				if (argName[0] == entry.shortName) {
					int ret = parseArgument(entry, argc - i - 1, &argv[i + 1]);
					if (ret < 0) return false;
					i += ret;

					goto next_loop;
				}
			}

			error = "Unknown shortNamed argument - " + std::string(argName);
			return false;
		}
		else {
			// Match by full name
			for (auto& entry : args) {
				if (!strcmp(entry.longName, argName)) {
					int ret = parseArgument(entry, argc - i - 1, &argv[i + 1]);
					if (ret < 0) return false;
					i += ret;

					goto next_loop;
				}
			}

			error = "Unknown argument - " + std::string(argName);
			return false;
		}

	next_loop:
		;
	}

	return true;
}

ProgramArgs::Args& ProgramArgs::getArgs(void) {
	return result;
}

std::string& ProgramArgs::getError(void) {
	return error;
}
