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

enum class line_type {
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
enum line_type get_line_type(string& line) {
    regex r(quote_include_regexp);
    if (regex_match(line, r)) {
        return line_type::QUOTE_DIRECTIVE;
    }
    regex r1(angle_include_regexp);
    if (regex_match(line, r1)) {
        return line_type::ANGLE_DIRECTIVE;
    }
    return line_type::NOT_DIRECTIVE;
}

/*
Возвращает include_filename из строки содержащей #include <filename> или #include "file" 
*/
string extract_file_path_from_directive(string& str_with_include_dir) {
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
void print_err_msg(const string& include_filename, const path& file_with_directives, int line_num) {
    cout << "unknown include file " <<  include_filename  << 
    " at file " << file_with_directives.string() << " at line " << line_num << endl; 
}

/*
Проверка существования файла inlcude_file из директивы #include <include_file> 
*/
path search_in_include_dirs(const path& include_file, const vector<path>& include_directories) {
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
path quote_file_search(const path& file_with_directives, const path& include_file, const vector<path>& include_directories) {
    path abs_include_file_path = file_with_directives.parent_path() / include_file;
    if (exists(abs_include_file_path)) {
        return abs_include_file_path;
    }
    return search_in_include_dirs(include_file, include_directories);
}

/*
Читает строки из потока in, привязанного к input_file, 
Если строка содержит #include<include_file_path> или #include<include_file_path>, 
подставляет содержимое файла, имеющего путь include_file_path, в out  */ 
bool read_and_substitute(const path& input_file, ifstream& in, ofstream& out, const vector<path>& include_directories) {
    string line; 
    int line_num = 1;
    while (getline(in, line)) {
        enum line_type line_type = get_line_type(line);
        if (line_type != line_type::NOT_DIRECTIVE ) {
            string include_filename = extract_file_path_from_directive(line);

            path include_file(include_filename); 
            if (line_type == line_type::QUOTE_DIRECTIVE) {
                include_file = quote_file_search(input_file, include_file, include_directories);
            }

            if (line_type == line_type::ANGLE_DIRECTIVE) {
                include_file = search_in_include_dirs(include_file, include_directories);
            }

            if (include_file.empty()) {
                print_err_msg(include_filename, input_file, line_num);
                return false;
            }
            
            ifstream in_included_file(include_file);
            read_and_substitute(include_file, in_included_file, out, include_directories); 
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
    ofstream out(out_file);
    return read_and_substitute(in_file, in, out, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
