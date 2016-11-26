#include <FileStream.hxx>

/* Little C++ quirck, which requires that non-const static variables are initialized out of line */
std::unordered_map<std::string, typename FileStreamSelector<StreamGoal::read>::type> FileStreamSelector<StreamGoal::read>::streamMap_{};
std::unordered_map<std::string, typename FileStreamSelector<StreamGoal::write>::type> FileStreamSelector<StreamGoal::write>::streamMap_{};
