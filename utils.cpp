#include "utils.hpp"
#include "globlecontext.hpp"

#include <cstring>

using namespace fits;

//static
int UID::uid = 0;

//static
int
UID::generate() {
	return uid++;
}

#if 0
foreground background
black        30         40
red          31         41
green        32         42
yellow       33         43
blue         34         44
magenta      35         45
cyan         36         46
white        37         47

reset             0  (everything back to normal)
bold / bright       1  (often a brighter shade of the same colour)
underline         4
inverse           7  (swap foregroundand background colours)
bold / bright off  21
underline off    24
inverse off      27
#endif
//static
const std::map<int, std::string> Utils::COLORS = {
	{-1, "\033[31mERROR:\033[0m"},
	{0, "\033[0m"},
	{1, "\033[32mINFO:\033[0m"},
	{2, "\033[33mWARN:\033[0m"},
	{7, "\033[0m"}
};

std::string Utils::trim(std::string& str) {
	std::string result = "";
	for (auto& e : str) {
		if (!std::isspace(e)) result += e;
	}
	return result;
}

CSVLoader::CSVLoader(const std::string& filename) : filename(filename) {
}

int
CSVLoader::load(RowCallback callback) {
	std::ifstream is(filename);
	if (!is) {
		Utils::log(1, "failed to open requirements file: %s\n", filename.data());
		return -1;
	}

	std::vector<char> buffer(1024, 0);
	std::vector<std::string> fields;
	int i = 0;
	while (!is.eof()) {
		std::memset(buffer.data(), 0, buffer.capacity());
		is.getline(buffer.data(), buffer.capacity());
		if (buffer.at(0) == '#' || buffer.at(0) == NULL) continue;

		std::string line(buffer.data());
		if (Utils::trim(line).empty()) continue;

		auto pos = std::string::npos;
		while (std::string::npos != (pos = line.find_first_of(','))) {
			std::string e = line.substr(0, pos);
			fields.push_back(e);
			line = line.substr(pos + 1);
		}
		if (line.size() > 0) fields.push_back(line);
		callback(i++, fields);
		fields.clear();
	}

	return 0;
}

CSVWriter::CSVWriter(const std::string & filename, const Row & columns/* = {}*/, bool append/* = true*/)
	: filename(filename), countOfColumns((int)columns.size()) {
	if (filename.empty()) {
		return;
	}

	if (append)
		ofs.open(filename, std::ofstream::app | std::ofstream::out);
	else
		ofs.open(filename, std::ofstream::out);
	if (!ofs) throw "failed to open file: " + filename;

	write(columns);
}

CSVWriter::~CSVWriter() {
	if (ofs) ofs.close();
}

int
CSVWriter::write(const Row & row) {
	int i = 0;
	for (; i < row.size() - 1; i++) {
		ofs << row.at(i) << ",";
	}
	ofs << row.at(i) << "\n";
	return 0;
}

int
CSVWriter::write(IndexedRow & row) {
	int i = 0;
	for (; i < countOfColumns - 1; i++) {
		ofs << (row.count(i) ? row[i] : "") << ",";
	}
	ofs << (row.count(i) ? row[i] : "") << "\n";
	return 0;
}