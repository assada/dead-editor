#pragma once

#include "Types.h"
#include "HandleTypes.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

using LanguageFactory = const TSLanguage* (*)();

struct LanguageConfig {
    std::string name;
    LanguageFactory factory;
    const char* query_source;
    std::string line_comment_token;
};

struct LanguageDefinition {
    std::string id;
    std::vector<std::string> extensions;
    std::vector<std::string> filenames;
    std::function<LanguageConfig()> config_factory;
};

struct LoadedLanguage {
    LanguageConfig config;
    TSQuery* query = nullptr;
    TSQueryPtr query_owned;
    std::vector<TokenType> capture_map;
};

class LanguageRegistry {
public:
    static LanguageRegistry& instance();

    void register_language(LanguageDefinition def);
    const LanguageDefinition* find_by_extension(const std::string& ext) const;
    const LanguageDefinition* find_by_filename(const std::string& filename) const;
    const LanguageDefinition* detect_language(const std::string& filepath) const;
    LoadedLanguage* get_or_load(const std::string& language_id);
    void unload(const std::string& language_id);
    void unload_all();
    bool is_loaded(const std::string& language_id) const;

private:
    LanguageRegistry() = default;
    ~LanguageRegistry();
    LanguageRegistry(const LanguageRegistry&) = delete;
    LanguageRegistry& operator=(const LanguageRegistry&) = delete;

    std::vector<LanguageDefinition> definitions_;
    std::unordered_map<std::string, std::string> ext_to_id_;
    std::unordered_map<std::string, std::string> filename_to_id_;
    std::unordered_map<std::string, std::unique_ptr<LoadedLanguage>> loaded_;

    void build_capture_map(LoadedLanguage& lang);
};

void register_all_languages();
