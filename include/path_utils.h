#ifndef PATHS_INCLUDED
#define PATHS_INCLUDED

#include <string>

/**
 * Create a file system path.
 *
 * @param dir String containing directory
 * @param base String containing base file name without extension
 * @param ext  String containing extension to use
 *
 * @note Tweak the separators and the function as necessary for each
 * operating system.
 */
std::string mkpath(const std::string& dir,
                   const std::string& base,
                   const std::string& ext);

#endif /* PATHS_INCLUDED */
