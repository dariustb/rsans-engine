#include <rsans_file_browser.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

fs::path currentDirectory() {
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    return ec ? fs::path("/") : cwd;
}

bool isDirectory(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

// Where to start: the folder of the current path if it exists, else the
// working directory
fs::path startingDirectory(const std::string& current) {
    if (!current.empty()) {
        const fs::path path = fs::absolute(expandHome(current));
        if (isDirectory(path)) {
            return path;
        }
        if (isDirectory(path.parent_path())) {
            return path.parent_path();
        }
    }
    return currentDirectory();
}

} // namespace

std::string expandHome(const std::string& path) {
    if (path == "~" || path.rfind("~/", 0) == 0) {
        const char* home = std::getenv("HOME");
        if (home != nullptr) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

FileBrowser::FileBrowser(std::vector<std::string> extensions, bool saveMode, const std::string& current)
: d_extensions(std::move(extensions))
, d_saveMode(saveMode) {
    if (d_saveMode && !current.empty() && !isDirectory(expandHome(current))) {
        d_query = fs::path(current).filename().string();
    }
    enterDirectory(startingDirectory(current));  // Normalizes it and reads it
}

void FileBrowser::refresh() {
    d_all.clear();
    d_error.clear();

    std::error_code ec;
    fs::directory_iterator it(d_directory, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        d_error = "Can't read folder: " + ec.message();
    } else {
        for (const fs::directory_entry& entry : it) {
            std::error_code typeError;
            Entry item;
            item.name = entry.path().filename().string();
            item.isDirectory = entry.is_directory(typeError);  // Follows symlinks
            d_all.push_back(std::move(item));
        }
    }

    // Folders first, then files, each sorted by name ignoring case
    std::sort(d_all.begin(), d_all.end(), [](const Entry& a, const Entry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory;
        }
        return toLower(a.name) < toLower(b.name);
    });
    applyFilter();
}

bool FileBrowser::matchesType(const std::string& name) const {
    if (d_extensions.empty() || d_showAllFiles) {
        return true;
    }
    const std::string extension = toLower(fs::path(name).extension().string());
    return std::find(d_extensions.begin(), d_extensions.end(), extension) != d_extensions.end();
}

void FileBrowser::applyFilter() {
    d_visible.clear();
    if (d_directory.has_parent_path() && d_directory != d_directory.root_path()) {
        d_visible.push_back(Entry{"..", true, true});
    }

    // In save mode the search box holds the file name, so don't filter by it
    const std::string search = d_saveMode ? std::string() : toLower(d_query);
    const bool showHidden = !d_query.empty() && d_query[0] == '.';
    for (const Entry& entry : d_all) {
        if (!showHidden && !entry.name.empty() && entry.name[0] == '.') {
            continue;
        }
        if (!entry.isDirectory && !matchesType(entry.name)) {
            continue;
        }
        if (!search.empty() && toLower(entry.name).find(search) == std::string::npos) {
            continue;
        }
        d_visible.push_back(entry);
    }
}

void FileBrowser::setQuery(const std::string& query) {
    d_query = query;
    applyFilter();
}

void FileBrowser::setShowAllFiles(bool showAll) {
    d_showAllFiles = showAll;
    applyFilter();
}

void FileBrowser::enterDirectory(const fs::path& directory) {
    d_directory = fs::absolute(directory).lexically_normal();
    // "a/b/" to "a/b", so the folder shown and its parent are right
    if (!d_directory.has_filename() && d_directory != d_directory.root_path()) {
        d_directory = d_directory.parent_path();
    }
    if (!d_saveMode) {
        d_query.clear();
    }
    refresh();
}

void FileBrowser::goUp() {
    if (d_directory != d_directory.root_path()) {
        enterDirectory(d_directory.parent_path());
    }
}

std::optional<std::string> FileBrowser::activate(std::size_t index) {
    if (index >= d_visible.size()) {
        return std::nullopt;
    }
    const Entry entry = d_visible[index];
    if (entry.isParent) {
        goUp();
        return std::nullopt;
    }
    const fs::path path = d_directory / entry.name;
    if (entry.isDirectory) {
        enterDirectory(path);
        return std::nullopt;
    }
    return path.string();
}

std::optional<std::string> FileBrowser::submitQuery() {
    const std::string typed = expandHome(d_query);

    // A typed path, absolute or relative to the current folder
    if (!typed.empty() && (typed.find('/') != std::string::npos || typed == "..")) {
        const fs::path path = fs::path(typed).is_absolute() ? fs::path(typed) : d_directory / typed;
        if (isDirectory(path)) {
            enterDirectory(path);
            if (d_saveMode) {
                d_query.clear();
                applyFilter();
            }
            return std::nullopt;
        }
        std::error_code ec;
        if (d_saveMode || fs::is_regular_file(path, ec)) {
            return path.lexically_normal().string();
        }
        return std::nullopt;
    }

    if (d_saveMode) {
        if (typed.empty()) {
            return std::nullopt;
        }
        const fs::path path = d_directory / typed;
        if (isDirectory(path)) {
            enterDirectory(path);
            d_query.clear();
            applyFilter();
            return std::nullopt;
        }
        return path.string();
    }

    // Open mode: the first match, skipping ".."
    for (std::size_t i = 0; i < d_visible.size(); ++i) {
        if (!d_visible[i].isParent) {
            return activate(i);
        }
    }
    return std::nullopt;
}
