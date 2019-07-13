#include <filesystem>
#include <iostream>
#include <string>
#include <functional>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

// Filters to apply to the file paths
using path_filter = std::function<bool(const fs::path &)>;


/*
 * Filters
 *
 * HOF that returnes a function that will select wether or not to include
 * a specfic file based on some criteria, usually filename
 */

/*
 * Include all files including hidden files, but not implicit files
 * . and ..
 */
path_filter all() {
  return [](fs::path p) {
    std::string ps = p.stem().string();
    return ps != "." && ps != "..";
  };
}

/*
 * Include all files including hidden and implicit files.
 */
path_filter all_with_implicit() {
  return []([[maybe_unused]] const fs::path & p) {
    return true;
  };
}

/*
 * Include files except those matching the string hide
 *
 * TODO(brian) define "matching"
 */
path_filter hide(std::string to_hide) {
  return [to_hide](const fs::path & p) {
    return p.stem().string() != to_hide;
  };
}

/*
 * Standard ls filter. Only show non-hidden files
 */
path_filter basic() {
  return [](fs::path p) {
    std::string ps = p.stem().string();
    return ps.front() != '.';
  };
}

struct file {
 public:
  file(fs::path path) : path(path) {
    if (stat(path.c_str(), &stats) < 0) {
      std::cerr << "Ahh"
#ifdef RELEASE
        " shucks";
#else
        " FUCK!";
#endif
      perror("stat creating struct file");
    }
  }

 public:
  /*
   * std::fs path object represending the file
   */
  fs::path path;

  /*
   * C stat structure representing additional information about the file
   * which is not included in the path object
   *
   * TODO(brian) system dependant, do I care about windows/OSX?
   */
  struct stat stats;

  /*
   * Character length of just the file name with any icons
   * Not to be confused with the number of bytes in the string
   *
   * Used for aligned printing
   */
  int formatted_length;
};

bool operator <(const timespec& lhs, const timespec& rhs)
{
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec < rhs.tv_nsec;
    else
        return lhs.tv_sec < rhs.tv_sec;
}

/*
 * Basic comparator prototype. given two file structs return true if the
 * lhs < rhs
 */
using file_comp = std::function<bool(const file&, const file&)>;

file_comp nil_comparator() {
  return [](auto f1, auto f2) { return 1; };
}

/*
 * Compare based on the filename stems
 */
file_comp filename(file_comp nested_comp) {
  return [nested_comp](auto f1, auto f2) {
    return f1.path.stem().string() < f2.path.stem().string() &&
      nested_comp(f1, f2);
  };
}

/*
 * Compare based on the file extensions
 */
file_comp extension(file_comp nested_comp) {
  return [nested_comp](auto f1, auto f2) {
    return f1.path.extension().string() < f2.path.extension().string() &&
      nested_comp(f1, f2);
  };
}

/*
 * Invert the current sorting order
 */
file_comp reverse(file_comp nested_comp) {
  return [nested_comp](auto f1, auto f2) {
    return !nested_comp(f1, f2);
  };
}

/*
 * Sort by modify time, the last time time file data was updated
 */
file_comp modify_time(file_comp nested_comp) {
  return [nested_comp](auto f1, auto f2) {
    return f1.stats.st_mtim < f2.stats.st_mtim && nested_comp(f1, f2);
  };
}

/*
 * Sort by change time, the last time the file data or metadata were
 * updated
 */
file_comp change_time(file_comp nested_comp) {
  return [nested_comp](auto f1, auto f2) {
    return f1.stats.st_ctim < f2.stats.st_ctim && nested_comp(f1, f2);
  };
}

/*
 * Sort by access time
 */
file_comp access_time(file_comp nested_comp) {
  return [nested_comp](auto f1, auto f2) {
    return f1.stats.st_atim < f2.stats.st_atim && nested_comp(f1, f2);
  };
}

/*
file_comp dirs_first(file_comp nested_comp) {
  return [nested_comp](auto f1, auto f2) {
    return 
  }
}
*/

/*
 * Print just the filename one file per row
 *
 * Default for non-tty and turned on with the -1 flag
 */
void print_single_short(const std::vector<file> & files) {
  for (auto& f: files) {
    std::cout << f.path.filename().string() << '\n';
  }
}

/*
 * Print just the filename with as many files per row as possible.
 *
 * Default for tty
 */
void print_rows_short(const std::vector<file> & files) {
  std::vector<int> row_widths;
  for (auto& f: files) {
    std::cout << f.path.filename().string() << ' ';
  }
}

int main() {
  auto it = fs::directory_iterator(".");

  std::vector<path_filter> path_filters = {basic()};
  std::vector<file> files;

  // Perform filtation operations, filters are composable and applied over
  // each path that is extracted from the directory
  for(auto& p: it) {
    if (std::all_of(path_filters.begin(), path_filters.end(),
                    [p](path_filter f){ return f(p); })) {
      files.emplace_back(p);
    }
  }

  // Sort

  // Display
  // TODO actually do different methods
  print_single_short(files);
  print_rows_short(files);
}
