#ifndef INCLUDED_RSANS_RHYME
#define INCLUDED_RSANS_RHYME

#include <string>
#include <unordered_map>
#include <vector>

class CMUDict {
  private:
    std::unordered_map<std::string, std::vector<std::string>> d_entries;
  public:
    std::vector<std::string> getPhonemes(const std::string& word) const;
    static std::vector<std::string> extractRhymeTail(const std::vector<std::string>& phonemes);
    static std::vector<std::string> extractVowels(const std::vector<std::string>& phonemes);
    static std::vector<std::string> extractConsonants(const std::vector<std::string>& phonemes);
    
    static bool isVowel(const std::string& phoneme);
    CMUDict(const std::string& dictPath);
};

// Union-Find for merging rhyme groups
class RhymeGrouper {
  private:
    std::unordered_map<int, int> parent_;
    std::unordered_map<int, int> rank_;
  public:
    int find(int x);
    void unite(int x, int y);
};

class ProjectData;

void detectRhymes(ProjectData& project);

#endif
