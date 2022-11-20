#pragma once

#include <map>
#include <string>
#include <vector>

class ProgramArgs {
	enum arg_type {
		ARG_TYPE_BOOL,
		ARG_TYPE_INT,
	};
	struct arg {
		char shortName;
		const char* longName;
		enum arg_type type;
	};

	using Args = std::map<const char*, unsigned long long>;
	std::vector<arg> args;
	Args result;

	std::string error;

	int parseArgument(struct arg& argument, int argsAvail, char** argv);

public:
	ProgramArgs();
	~ProgramArgs();

	void addIntParam(char shortName, const char* longName, int init);
	void addBoolParam(char shortName, const char* longName);

	bool parse(int argc, char** argv);
	Args& getArgs(void);
	std::string& getError(void);
};

