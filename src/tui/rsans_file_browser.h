#ifndef INCLUDED_RSANS_FILE_BROWSER
#define INCLUDED_RSANS_FILE_BROWSER

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// The state behind the file dialog: a current folder, its entries, and a
// type-to-filter search. Independent of FTXUI so it can be tested.
//
// In open mode a file is chosen from the list. In save mode (for an output
// file) the search text is also the name to save as, so a file that doesn't
// exist yet can be chosen.
class FileBrowser {
  public:
    struct Entry {
        std::string name;
        bool isDirectory = false;
        bool isParent = false;  // The ".." entry
    };

    // extensions: file types to list, lowercase with the dot (e.g. ".wav");
    //   empty lists every file. Folders are always listed.
    // current: the field's current path, used to pick the starting folder
    //   (and, in save mode, the starting file name).
    FileBrowser(std::vector<std::string> extensions, bool saveMode, const std::string& current);

    const std::filesystem::path& directory() const { return d_directory; }
    const std::vector<Entry>& entries() const { return d_visible; }  // Filtered, ".." first
    const std::string& error() const { return d_error; }             // e.g. permission denied
    bool saveMode() const { return d_saveMode; }
    bool hasExtensionFilter() const { return !d_extensions.empty(); }
    const std::vector<std::string>& extensions() const { return d_extensions; }

    // Type-to-filter text: entries whose name contains it, ignoring case.
    // Hidden (dot) files are shown only when it starts with '.'.
    const std::string& query() const { return d_query; }
    void setQuery(const std::string& query);

    // List every file, not only the matching file types
    bool showAllFiles() const { return d_showAllFiles; }
    void setShowAllFiles(bool showAll);

    void goUp();
    void enterDirectory(const std::filesystem::path& directory);
    void refresh();  // Re-reads the current folder

    // Enter on a list entry: opens a folder (returns nothing) or chooses a
    // file (returns its full path)
    std::optional<std::string> activate(std::size_t index);

    // Enter in the search box. A typed path to a folder opens it; to a file
    // chooses it. Otherwise, open mode activates the first entry, and save
    // mode chooses "<folder>/<search text>".
    std::optional<std::string> submitQuery();

  private:
    std::vector<std::string> d_extensions;
    bool d_saveMode;
    std::filesystem::path d_directory;
    std::vector<Entry> d_all;
    std::vector<Entry> d_visible;
    std::string d_query;
    bool d_showAllFiles = false;
    std::string d_error;

    void applyFilter();
    bool matchesType(const std::string& name) const;
};

// Expands a leading "~" to the home directory
std::string expandHome(const std::string& path);

#endif
