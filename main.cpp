#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>

using namespace std; 
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

enum class LineType {
    NOT_DIRECTIVE,
    QUOTE_DIRECTIVE,
    ANGLE_DIRECTIVE, 
};

static const string quote_include_regexp = "\\s*#\\s*include\\s*\"([^\"]*)\"\\s*";
static const string angle_include_regexp = "\\s*#\\s*include\\s*<([^>]*)>\\s*";

/*
Определяет тип строки из файла с директивами #include. 
Если строка содержит #include директиву, возвращает тип директивы 
(#include "" или #inlcude <>) */
enum LineType GetLineType(string& line) {
    regex r(quote_include_regexp);
    if (regex_match(line, r)) {
        return LineType::QUOTE_DIRECTIVE;
    }
    regex r1(angle_include_regexp);
    if (regex_match(line, r1)) {
        return LineType::ANGLE_DIRECTIVE;
    }
    return LineType::NOT_DIRECTIVE;
}

/*
Возвращает include_filename из строки содержащей #include <filename> или #include "file" 
*/
string ExtractFilePathFromDirective(string& str_with_include_dir) {
    string include_filename; 
    auto first_quote_or_angle = find_if(str_with_include_dir.begin(), str_with_include_dir.end(), [](char c){return (c == '\"') || (c == '<');});
    for (auto it = first_quote_or_angle + 1; it != str_with_include_dir.end(); it++) {
        if ((*it == '\"') || (*it == '>')) {
            break;
        }
        include_filename +=  *it; 
    }
    return include_filename; 
}

/*
Ошибка: файл include_filename из #include директивы(приведенной в file_with_directives в строке 
line_num) не существует
*/
void PrintErrMsg(const string& include_filename, const path& file_with_directives, int line_num) {
    cout << "unknown include file " <<  include_filename  << 
    " at file " << file_with_directives.string() << " at line " << line_num << endl; 
}


/*
Проверка существования файла inlcude_file из директивы #include <include_file> 
*/
path SearchInIncludeDirs(const path& include_file, const vector<path>& include_directories) {
    for (auto dir : include_directories) {
        path abs_inclide_file_path = dir / include_file;
        if (exists(abs_inclide_file_path)) {
            return abs_inclide_file_path; 
        }
    }
    return path();
}

/*
Проверка существования файла include_file из директивы #include "include_file",
содержащейся в файле с путем file_with_directives
Сначала include_file ищется в директории, в которой хранится file_with_directives
Если не нашелся, ищется в векторе include_directories
*/
path QuoteFileSearch(const path& file_with_directives, const path& include_file, const vector<path>& include_directories) {
    path abs_include_file_path = file_with_directives.parent_path() / include_file;
    if (exists(abs_include_file_path)) {
        return abs_include_file_path;
    }
    return SearchInIncludeDirs(include_file, include_directories);
}


/*
Читает строки из потока in, привязанного к input_file, 
Если строка содержит #include<include_file_path> или #include<include_file_path>, 
подставляет содержимое файла, имеющего путь include_file_path, в out  */ 
bool ReadAndSubstitute(const path& input_file, ifstream& in, ofstream& out, const vector<path>& include_directories) {
    string line; 
    int line_num = 1;
    while (getline(in, line)) {
        enum LineType line_type = GetLineType(line);
        if (line_type != LineType::NOT_DIRECTIVE ) {
            string include_filename = ExtractFilePathFromDirective(line);

            path include_file(include_filename); 
            if (line_type == LineType::QUOTE_DIRECTIVE) {
                include_file = QuoteFileSearch(input_file, include_file, include_directories);
            }

            if (line_type == LineType::ANGLE_DIRECTIVE) {
                include_file = SearchInIncludeDirs(include_file, include_directories);
            }

            if (include_file.empty()) {
                PrintErrMsg(include_filename, input_file, line_num);
                return false;
            }
            
            ifstream in_included_file(include_file);
            ReadAndSubstitute(include_file, in_included_file, out, include_directories); 
        } else {
            out << line << endl; 
        }
        line_num++;
    }
    return true; 
}

/* Исполняет #include директивы */
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);
    if (!in.is_open()) {
        cout << "Failed to open input file: " << in_file << endl;
        return false;
    }
    ofstream out(out_file);
    if (!out.is_open()) {
        cout << "Failed to open output file: " << out_file << endl;
        return false; 
    }
    return ReadAndSubstitute(in_file, in, out, include_directories);
}
