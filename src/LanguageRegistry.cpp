#include "LanguageRegistry.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

extern "C" const TSLanguage* tree_sitter_cpp();
extern "C" const TSLanguage* tree_sitter_c();
extern "C" const TSLanguage* tree_sitter_python();
// extern "C" const TSLanguage* tree_sitter_markdown();
// extern "C" const TSLanguage* tree_sitter_markdown_inline();
extern "C" const TSLanguage* tree_sitter_yaml();
extern "C" const TSLanguage* tree_sitter_lua();
extern "C" const TSLanguage* tree_sitter_zig();
extern "C" const TSLanguage* tree_sitter_diff();
extern "C" const TSLanguage* tree_sitter_meson();
extern "C" const TSLanguage* tree_sitter_toml();
extern "C" const TSLanguage* tree_sitter_json();
extern "C" const TSLanguage* tree_sitter_javascript();
extern "C" const TSLanguage* tree_sitter_html();
extern "C" const TSLanguage* tree_sitter_css();
extern "C" const TSLanguage* tree_sitter_php();
extern "C" const TSLanguage* tree_sitter_bash();
extern "C" const TSLanguage* tree_sitter_rust();
extern "C" const TSLanguage* tree_sitter_ini();

static const std::unordered_map<std::string, TokenType> CAPTURE_NAME_TO_TYPE = {
    {"comment", TokenType::Comment},
    {"string", TokenType::String},
    {"string.special", TokenType::String},
    {"char", TokenType::Char},
    {"character", TokenType::Char},
    {"number", TokenType::Number},
    {"float", TokenType::Number},
    {"integer", TokenType::Number},
    {"type", TokenType::Type},
    {"type.builtin", TokenType::Type},
    {"type.definition", TokenType::Type},
    {"keyword", TokenType::Keyword},
    {"keyword.control", TokenType::Keyword},
    {"keyword.function", TokenType::Keyword},
    {"keyword.operator", TokenType::Keyword},
    {"keyword.return", TokenType::Keyword},
    {"keyword.conditional", TokenType::Keyword},
    {"keyword.repeat", TokenType::Keyword},
    {"keyword.import", TokenType::Keyword},
    {"keyword.exception", TokenType::Keyword},
    {"keyword.modifier", TokenType::Keyword},
    {"keyword.storage", TokenType::Keyword},
    {"preprocessor", TokenType::Preprocessor},
    {"preproc", TokenType::Preprocessor},
    {"include", TokenType::Preprocessor},
    {"define", TokenType::Preprocessor},
    {"function", TokenType::Function},
    {"function.call", TokenType::Function},
    {"function.builtin", TokenType::Function},
    {"function.method", TokenType::Function},
    {"function.macro", TokenType::Function},
    {"method", TokenType::Function},
    {"method.call", TokenType::Function},
    {"variable", TokenType::Variable},
    {"variable.builtin", TokenType::Constant},
    {"variable.parameter", TokenType::Parameter},
    {"parameter", TokenType::Parameter},
    {"property", TokenType::Property},
    {"field", TokenType::Property},
    {"attribute", TokenType::Attribute},
    {"attribute.builtin", TokenType::Attribute},
    {"decorator", TokenType::Attribute},
    {"operator", TokenType::Operator},
    {"punctuation", TokenType::Punctuation},
    {"punctuation.bracket", TokenType::Punctuation},
    {"punctuation.delimiter", TokenType::Punctuation},
    {"punctuation.special", TokenType::Punctuation},
    {"constant", TokenType::Constant},
    {"constant.builtin", TokenType::Constant},
    {"boolean", TokenType::Constant},
    {"label", TokenType::Label},
    {"namespace", TokenType::Namespace},
    {"module", TokenType::Namespace},
    {"constructor", TokenType::Type},
    {"tag", TokenType::Tag},
    {"tag.attribute", TokenType::Attribute},
    {"text", TokenType::Default},
    {"text.title", TokenType::Function},
    {"text.emphasis", TokenType::String},
    {"text.strong", TokenType::Keyword},
    {"text.literal", TokenType::String},
    {"text.uri", TokenType::String},
    {"markup.heading", TokenType::Function},
    {"markup.bold", TokenType::Keyword},
    {"markup.italic", TokenType::String},
    {"markup.link", TokenType::String},
    {"markup.raw", TokenType::String},
    {"escape", TokenType::Constant},
};

constexpr const char* CPP_QUERY = R"scm(
(comment) @comment
(string_literal) @string
(raw_string_literal) @string
(system_lib_string) @string
(char_literal) @char
(number_literal) @number

(primitive_type) @type
(sized_type_specifier) @type
(type_identifier) @type
(auto) @type

(preproc_include) @preprocessor
(preproc_def) @preprocessor
(preproc_function_def) @preprocessor
(preproc_if) @preprocessor
(preproc_ifdef) @preprocessor

(call_expression function: (identifier) @function)
(call_expression function: (qualified_identifier name: (identifier) @function))
(call_expression function: (field_expression field: (field_identifier) @function))
(function_declarator declarator: (identifier) @function)
(function_declarator declarator: (qualified_identifier name: (identifier) @function))
(function_declarator declarator: (field_identifier) @function)
(template_function name: (identifier) @function)
(template_method name: (field_identifier) @function)

(field_identifier) @property

(namespace_identifier) @namespace

(this) @variable.builtin
(null "nullptr" @constant)
(true) @constant
(false) @constant

[
  "catch" "class" "co_await" "co_return" "co_yield" "constexpr" "constinit"
  "consteval" "delete" "explicit" "final" "friend" "mutable" "namespace"
  "noexcept" "new" "override" "private" "protected" "public" "template"
  "throw" "try" "typename" "using" "concept" "requires" "virtual"
  "if" "else" "for" "while" "do" "switch" "case" "default"
  "break" "continue" "return" "goto"
  "struct" "union" "enum" "static" "extern" "inline" "const" "volatile" "typedef"
] @keyword
)scm";

constexpr const char* C_QUERY = R"scm(
(comment) @comment
(string_literal) @string
(system_lib_string) @string
(char_literal) @char
(number_literal) @number

(primitive_type) @type
(sized_type_specifier) @type
(type_identifier) @type

(preproc_include) @preprocessor
(preproc_def) @preprocessor
(preproc_function_def) @preprocessor
(preproc_if) @preprocessor
(preproc_ifdef) @preprocessor
(preproc_directive) @preprocessor

(call_expression function: (identifier) @function)
(call_expression function: (field_expression field: (field_identifier) @function))
(function_declarator declarator: (identifier) @function)
(preproc_function_def name: (identifier) @function)

(field_identifier) @property
(statement_identifier) @label

(null) @constant
(true) @constant
(false) @constant

[
  "break" "case" "const" "continue" "default" "do" "else" "enum"
  "extern" "for" "if" "inline" "return" "sizeof" "static" "struct"
  "switch" "typedef" "union" "volatile" "while" "goto" "register"
] @keyword
)scm";

constexpr const char* PYTHON_QUERY = R"scm(
(comment) @comment
(string) @string
(escape_sequence) @escape
(integer) @number
(float) @number

(decorator) @function
(decorator (identifier) @function)
(call function: (attribute attribute: (identifier) @function))
(call function: (identifier) @function)
(function_definition name: (identifier) @function)

(attribute attribute: (identifier) @property)
(type (identifier) @type)

(none) @constant
(true) @constant
(false) @constant

[
  "as" "assert" "async" "await" "break" "class" "continue" "def"
  "del" "elif" "else" "except" "finally" "for" "from" "global"
  "if" "import" "lambda" "nonlocal" "pass" "raise" "return"
  "try" "while" "with" "yield" "match" "case" "and" "in" "is"
  "not" "or"
] @keyword

(identifier) @variable
)scm";

constexpr const char* MARKDOWN_QUERY = R"scm(
(atx_heading) @function
(setext_heading) @function
(fenced_code_block) @string
(indented_code_block) @string
(block_quote) @comment
(thematic_break) @operator
)scm";

constexpr const char* YAML_QUERY = R"scm(
(comment) @comment
(string_scalar) @string
(single_quote_scalar) @string
(double_quote_scalar) @string
(block_scalar) @string
(escape_sequence) @escape
(integer_scalar) @number
(float_scalar) @number
(boolean_scalar) @constant
(null_scalar) @constant
(anchor) @label
(alias) @label
(tag) @type
(block_mapping_pair key: (_) @property)
(flow_mapping key: (_) @property)

["[" "]" "{" "}"] @punctuation.bracket
[":" "," "-" ">" "|"] @punctuation.delimiter
["&" "*"] @operator
)scm";

constexpr const char* LUA_QUERY = R"scm(
(comment) @comment
(string) @string
(number) @number

(function_declaration name: (identifier) @function)
(function_call name: (identifier) @function)
(method_index_expression method: (identifier) @function)
(function_definition) @function

(parameters (identifier) @parameter)

(dot_index_expression field: (identifier) @property)
(bracket_index_expression) @property
(field name: (identifier) @property)
(table_constructor) @variable

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
["," ";" "."] @punctuation.delimiter
[":"] @punctuation

["=" "+" "-" "*" "/" "%" "^" "#" "==" "~=" "<" ">" "<=" ">=" ".."] @operator
["and" "or" "not"] @operator

[
  "do" "else" "elseif" "end" "for" "function"
  "goto" "if" "in" "local" "repeat" "return"
  "then" "until" "while" "break"
] @keyword

(nil) @constant
(true) @constant
(false) @constant
(vararg_expression) @variable

(identifier) @variable
)scm";

constexpr const char* ZIG_QUERY = R"scm(
(comment) @comment
(string) @string
(multiline_string) @string
(character) @char
(escape_sequence) @escape
(integer) @number
(float) @number

(function_declaration name: (identifier) @function)
(builtin_function) @function

(function_call) @function
(call_expression function: (identifier) @function)

(parameter (identifier) @parameter)

(field_access field: (identifier) @property)
(struct_initialization field: (identifier) @property)

(primitive_type) @type
(type_identifier) @type

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
[";" ":" "," "."] @punctuation.delimiter
["=" "+=" "-=" "*=" "/=" "%="] @operator
["+" "-" "*" "/" "%" "&" "|" "^" "~" "!" "and" "or"] @operator
["<" ">" "<=" ">=" "==" "!=" "<<" ">>"] @operator
["=>" ".."] @operator

[
  "fn" "pub" "const" "var" "comptime" "inline" "noinline" "extern"
  "if" "else" "switch" "while" "for" "break" "continue" "return"
  "try" "catch" "defer" "errdefer" "orelse" "async" "await" "suspend" "resume"
  "struct" "enum" "union" "error" "test" "usingnamespace" "unreachable"
] @keyword

(boolean) @constant
(null) @constant
(undefined) @constant

(identifier) @variable
)scm";

constexpr const char* DIFF_QUERY = R"scm(
(command) @preprocessor
(location) @function
(addition) @string
(deletion) @comment
(context) @variable
(file_change) @type
)scm";

constexpr const char* MESON_QUERY = R"scm(
(comment) @comment
(string) @string
(number) @number
(identifier) @variable

(normal_command command: (identifier) @function)
(pair key: (identifier) @variable)
(pair key: (string) @variable)

(bool) @keyword
[
  "if" "elif" "else" "endif" "foreach" "endforeach"
  "and" "or" "not" "in"
] @keyword
)scm";

constexpr const char* TOML_QUERY = R"scm(
(comment) @comment
(string) @string
(integer) @number
(float) @number
(boolean) @constant
(local_date) @constant
(local_time) @constant
(local_date_time) @constant
(offset_date_time) @constant

(bare_key) @property
(dotted_key) @property
(quoted_key) @string
(table (bare_key) @namespace)
(table (dotted_key) @namespace)
(table_array_element (bare_key) @namespace)
(table_array_element (dotted_key) @namespace)

["[" "]" "[[" "]]" "{" "}"] @punctuation.bracket
["." "," "="] @punctuation.delimiter
)scm";

constexpr const char* JSON_QUERY = R"scm(
(comment) @comment
(string) @string
(number) @number
(pair key: (string) @property)
(escape_sequence) @escape

["[" "]" "{" "}"] @punctuation.bracket
[":" ","] @punctuation.delimiter

(null) @constant
(true) @constant
(false) @constant
)scm";

constexpr const char* JAVASCRIPT_QUERY = R"scm(
(identifier) @variable
(property_identifier) @property

(function_expression name: (identifier) @function)
(function_declaration name: (identifier) @function)
(method_definition name: (property_identifier) @function)
(call_expression function: (identifier) @function)
(call_expression function: (member_expression property: (property_identifier) @function))

(this) @variable.builtin
(super) @variable.builtin

[
  (true)
  (false)
  (null)
  (undefined)
] @constant

(comment) @comment

[
  (string)
  (template_string)
] @string

(regex) @string
(number) @number

[
  "as" "async" "await" "break" "case" "catch" "class" "const" "continue"
  "debugger" "default" "delete" "do" "else" "export" "extends" "finally"
  "for" "from" "function" "get" "if" "import" "in" "instanceof" "let"
  "new" "of" "return" "set" "static" "switch" "target" "throw" "try"
  "typeof" "var" "void" "while" "with" "yield"
] @keyword
)scm";

constexpr const char* HTML_QUERY = R"scm(
(comment) @comment
(tag_name) @tag
(attribute_name) @attribute
(attribute_value) @string
(quoted_attribute_value) @string
(doctype) @preprocessor
(erroneous_end_tag_name) @comment

["<" ">" "</" "/>"] @punctuation.bracket
["="] @operator
)scm";

constexpr const char* CSS_QUERY = R"scm(
(comment) @comment
(string_value) @string
(integer_value) @number
(float_value) @number
(color_value) @constant
(plain_value) @variable

(tag_name) @tag
(class_name) @type
(id_name) @function
(property_name) @property
(feature_name) @property
(attribute_name) @attribute
(function_name) @function

(pseudo_class_selector (class_name) @function)
(pseudo_element_selector (tag_name) @function)

(unit) @constant

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
[";" ":" ","] @punctuation.delimiter
[">" "~" "+" "*"] @operator

(important) @keyword
(at_keyword) @preprocessor

(namespace_name) @namespace
)scm";

constexpr const char* PHP_QUERY = R"scm(
[
  (php_tag)
] @tag

(comment) @comment

[
  (string)
  (string_content)
  (encapsed_string)
  (heredoc)
  (heredoc_body)
  (nowdoc_body)
] @string

(boolean) @constant
(null) @constant
(integer) @number
(float) @number

(primitive_type) @type
(cast_type) @type
(named_type) @type

(namespace_definition name: (namespace_name (name) @namespace))
(namespace_name (name) @namespace)

(variable_name) @variable

(method_declaration name: (name) @function)
(function_call_expression function: (name) @function)
(scoped_call_expression name: (name) @function)
(member_call_expression name: (name) @function)
(function_definition name: (name) @function)

(property_element (variable_name) @property)
(member_access_expression name: (name) @property)

[
  "and" "as" "break" "case" "catch" "class" "clone" "const" "continue"
  "declare" "default" "do" "echo" "else" "elseif" "enddeclare" "endfor"
  "endforeach" "endif" "endswitch" "endwhile" "enum" "exit" "extends"
  "finally" "fn" "for" "foreach" "function" "global" "goto" "if"
  "implements" "include" "include_once" "instanceof" "insteadof" "interface"
  "match" "namespace" "new" "or" "print" "require" "require_once" "return"
  "switch" "throw" "trait" "try" "use" "while" "xor" "yield"
  (abstract_modifier)
  (final_modifier)
  (readonly_modifier)
  (static_modifier)
  (visibility_modifier)
] @keyword
)scm";

constexpr const char* BASH_QUERY = R"scm(
(comment) @comment

[
  (string)
  (raw_string)
  (heredoc_body)
  (heredoc_start)
] @string

(command_name) @function
(function_definition name: (word) @function)

(variable_name) @property

(file_descriptor) @number

[
  (command_substitution)
  (process_substitution)
  (expansion)
] @variable

[
  "case" "do" "done" "elif" "else" "esac" "export" "fi" "for" "function"
  "if" "in" "select" "then" "unset" "until" "while"
] @keyword
)scm";

constexpr const char* RUST_QUERY = R"scm(
(line_comment) @comment
(block_comment) @comment

(char_literal) @string
(string_literal) @string
(raw_string_literal) @string
(escape_sequence) @escape

(integer_literal) @number
(float_literal) @number
(boolean_literal) @constant

(type_identifier) @type
(primitive_type) @type

(call_expression function: (identifier) @function)
(call_expression function: (field_expression field: (field_identifier) @function))
(call_expression function: (scoped_identifier name: (identifier) @function))
(generic_function function: (identifier) @function)
(generic_function function: (scoped_identifier name: (identifier) @function))
(generic_function function: (field_expression field: (field_identifier) @function))
(macro_invocation macro: (identifier) @function)
(function_item (identifier) @function)
(function_signature_item (identifier) @function)

(parameter (identifier) @parameter)
(lifetime (identifier) @label)
(field_identifier) @property

(attribute_item) @attribute
(inner_attribute_item) @attribute

(crate) @keyword
(mutable_specifier) @keyword
(super) @keyword
(self) @variable.builtin

"as" @keyword
"async" @keyword
"await" @keyword
"break" @keyword
"const" @keyword
"continue" @keyword
"default" @keyword
"dyn" @keyword
"else" @keyword
"enum" @keyword
"extern" @keyword
"fn" @keyword
"for" @keyword
"if" @keyword
"impl" @keyword
"in" @keyword
"let" @keyword
"loop" @keyword
"match" @keyword
"mod" @keyword
"move" @keyword
"pub" @keyword
"ref" @keyword
"return" @keyword
"static" @keyword
"struct" @keyword
"trait" @keyword
"type" @keyword
"union" @keyword
"unsafe" @keyword
"use" @keyword
"where" @keyword
"while" @keyword
"yield" @keyword
)scm";

constexpr const char* INI_QUERY = R"scm(
(comment) @comment
(section_name (text) @type)
(setting (setting_name) @variable)
["[" "]"] @operator
"=" @operator
)scm";

LanguageRegistry::~LanguageRegistry() {
    unload_all();
}

LanguageRegistry& LanguageRegistry::instance() {
    static LanguageRegistry registry;
    return registry;
}

void LanguageRegistry::register_language(LanguageDefinition def) {
    for (const auto& ext : def.extensions) {
        ext_to_id_[ext] = def.id;
    }
    for (const auto& fname : def.filenames) {
        filename_to_id_[fname] = def.id;
    }
    definitions_.push_back(std::move(def));
}

const LanguageDefinition* LanguageRegistry::find_by_extension(const std::string& ext) const {
    auto it = ext_to_id_.find(ext);
    if (it == ext_to_id_.end()) return nullptr;
    for (const auto& def : definitions_) {
        if (def.id == it->second) return &def;
    }
    return nullptr;
}

const LanguageDefinition* LanguageRegistry::find_by_filename(const std::string& filename) const {
    auto it = filename_to_id_.find(filename);
    if (it == filename_to_id_.end()) return nullptr;
    for (const auto& def : definitions_) {
        if (def.id == it->second) return &def;
    }
    return nullptr;
}

const LanguageDefinition* LanguageRegistry::detect_language(const std::string& filepath) const {
    size_t last_slash = filepath.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? filepath.substr(last_slash + 1) : filepath;

    const LanguageDefinition* def = find_by_filename(filename);
    if (def) return def;

    size_t dot_pos = filename.rfind('.');
    if (dot_pos != std::string::npos && dot_pos < filename.size() - 1) {
        std::string ext = filename.substr(dot_pos + 1);
        def = find_by_extension(ext);
        if (def) return def;
    }

    return nullptr;
}

void LanguageRegistry::build_capture_map(LoadedLanguage& lang) {
    if (!lang.query) return;

    uint32_t capture_count = ts_query_capture_count(lang.query);
    lang.capture_map.resize(capture_count, TokenType::Default);

    for (uint32_t i = 0; i < capture_count; i++) {
        uint32_t len;
        const char* name = ts_query_capture_name_for_id(lang.query, i, &len);
        std::string capture_name(name, len);

        auto it = CAPTURE_NAME_TO_TYPE.find(capture_name);
        if (it != CAPTURE_NAME_TO_TYPE.end()) {
            lang.capture_map[i] = it->second;
        } else {
            size_t dot_pos = capture_name.find('.');
            if (dot_pos != std::string::npos) {
                std::string base = capture_name.substr(0, dot_pos);
                it = CAPTURE_NAME_TO_TYPE.find(base);
                if (it != CAPTURE_NAME_TO_TYPE.end()) {
                    lang.capture_map[i] = it->second;
                }
            }
        }
    }
}

LoadedLanguage* LanguageRegistry::get_or_load(const std::string& language_id) {
    auto it = loaded_.find(language_id);
    if (it != loaded_.end()) {
        return it->second.get();
    }

    const LanguageDefinition* def = nullptr;
    for (const auto& d : definitions_) {
        if (d.id == language_id) {
            def = &d;
            break;
        }
    }
    if (!def) return nullptr;

    auto loaded = std::make_unique<LoadedLanguage>();
    loaded->config = def->config_factory();

    uint32_t error_offset;
    TSQueryError error_type;
    loaded->query_owned.reset(ts_query_new(
        loaded->config.factory(),
        loaded->config.query_source,
        static_cast<uint32_t>(strlen(loaded->config.query_source)),
        &error_offset,
        &error_type
    ));
    loaded->query = loaded->query_owned.get();

    if (!loaded->query) {
        fprintf(stderr, "Query compilation error for %s at offset %u, type %d\n",
                language_id.c_str(), error_offset, error_type);
    } else {
        build_capture_map(*loaded);
    }

    LoadedLanguage* ptr = loaded.get();
    loaded_[language_id] = std::move(loaded);
    return ptr;
}

void LanguageRegistry::unload(const std::string& language_id) {
    loaded_.erase(language_id);
}

void LanguageRegistry::unload_all() {
    loaded_.clear();
}

bool LanguageRegistry::is_loaded(const std::string& language_id) const {
    return loaded_.find(language_id) != loaded_.end();
}

void register_all_languages() {
    LanguageRegistry& registry = LanguageRegistry::instance();

    registry.register_language({
        "cpp",
        {"cpp", "cc", "cxx", "hpp", "hxx", "h", "hh", "ipp", "tpp"},
        {},
        []() -> LanguageConfig {
            return {
                "cpp",
                tree_sitter_cpp,
                CPP_QUERY,
                "//",
                {"/*", "*/"},
                DEFAULT_AUTO_PAIRS,
                {'{'}
            };
        }
    });

    registry.register_language({
        "c",
        {"c"},
        {},
        []() -> LanguageConfig {
            return {
                "c",
                tree_sitter_c,
                C_QUERY,
                "//",
                {"/*", "*/"},
                DEFAULT_AUTO_PAIRS,
                {'{'}
            };
        }
    });

    registry.register_language({
        "python",
        {"py", "pyw", "pyi"},
        {},
        []() -> LanguageConfig {
            return {
                "python",
                tree_sitter_python,
                PYTHON_QUERY,
                "#",
                {"\"\"\"", "\"\"\""},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}},
                {':'}
            };
        }
    });

    // FIXME: tree-sitter-markdown from mikkihugo/update-tree-sitter-0.25 branch
    // still crashes with SIGSEGV on some markdown files (small files with lists)
    // PR: https://github.com/tree-sitter-grammars/tree-sitter-markdown/pull/207
    // registry.register_language({
    //     "markdown",
    //     {"md", "markdown", "mkd", "mkdn"},
    //     {"README", "CHANGELOG"},
    //     []() -> LanguageConfig {
    //         return {"markdown", tree_sitter_markdown, MARKDOWN_QUERY, "", {}, {}, {}};
    //     }
    // });

    registry.register_language({
        "yaml",
        {"yaml", "yml"},
        {},
        []() -> LanguageConfig {
            return {
                "yaml",
                tree_sitter_yaml,
                YAML_QUERY,
                "#",
                {},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}},
                {':'}
            };
        }
    });

    registry.register_language({
        "lua",
        {"lua"},
        {},
        []() -> LanguageConfig {
            return {
                "lua",
                tree_sitter_lua,
                LUA_QUERY,
                "--",
                {"--[[", "]]"},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}},
                {}
            };
        }
    });

    registry.register_language({
        "zig",
        {"zig"},
        {},
        []() -> LanguageConfig {
            return {
                "zig",
                tree_sitter_zig,
                ZIG_QUERY,
                "//",
                {},
                DEFAULT_AUTO_PAIRS,
                {'{'}
            };
        }
    });

    registry.register_language({
        "diff",
        {"diff", "patch"},
        {},
        []() -> LanguageConfig {
            return {
                "diff",
                tree_sitter_diff,
                DIFF_QUERY,
                "",
                {},
                {},
                {}
            };
        }
    });

    registry.register_language({
        "meson",
        {},
        {"meson.build", "meson_options.txt"},
        []() -> LanguageConfig {
            return {
                "meson",
                tree_sitter_meson,
                MESON_QUERY,
                "#",
                {},
                {{'(', ')'}, {'[', ']'}, {'\'', '\''}},
                {}
            };
        }
    });

    registry.register_language({
        "toml",
        {"toml"},
        {"Cargo.toml", "pyproject.toml"},
        []() -> LanguageConfig {
            return {
                "toml",
                tree_sitter_toml,
                TOML_QUERY,
                "#",
                {},
                {{'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}},
                {}
            };
        }
    });

    registry.register_language({
        "json",
        {"json", "jsonc"},
        {"package.json", "tsconfig.json", ".prettierrc", ".eslintrc"},
        []() -> LanguageConfig {
            return {
                "json",
                tree_sitter_json,
                JSON_QUERY,
                "",
                {},
                {{'[', ']'}, {'{', '}'}, {'"', '"'}},
                {'{', '['}
            };
        }
    });

    registry.register_language({
        "javascript",
        {"js", "mjs", "cjs", "jsx"},
        {},
        []() -> LanguageConfig {
            return {
                "javascript",
                tree_sitter_javascript,
                JAVASCRIPT_QUERY,
                "//",
                {"/*", "*/"},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}, {'`', '`'}},
                {'{'}
            };
        }
    });

    registry.register_language({
        "html",
        {"html", "htm", "xhtml"},
        {},
        []() -> LanguageConfig {
            return {
                "html",
                tree_sitter_html,
                HTML_QUERY,
                "",
                {"<!--", "-->"},
                {{'<', '>'}, {'"', '"'}, {'\'', '\''}},
                {}
            };
        }
    });

    registry.register_language({
        "css",
        {"css"},
        {},
        []() -> LanguageConfig {
            return {
                "css",
                tree_sitter_css,
                CSS_QUERY,
                "",
                {"/*", "*/"},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}},
                {'{'}
            };
        }
    });

    registry.register_language({
        "php",
        {"php", "phtml", "php3", "php4", "php5", "php7", "phps"},
        {},
        []() -> LanguageConfig {
            return {
                "php",
                tree_sitter_php,
                PHP_QUERY,
                "//",
                {"/*", "*/"},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}},
                {'{'}
            };
        }
    });

    registry.register_language({
        "bash",
        {"sh", "bash", "zsh", "ksh"},
        {".bashrc", ".bash_profile", ".zshrc", ".profile"},
        []() -> LanguageConfig {
            return {
                "bash",
                tree_sitter_bash,
                BASH_QUERY,
                "#",
                {},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}, {'`', '`'}},
                {}
            };
        }
    });

    registry.register_language({
        "rust",
        {"rs"},
        {},
        []() -> LanguageConfig {
            return {
                "rust",
                tree_sitter_rust,
                RUST_QUERY,
                "//",
                {"/*", "*/"},
                {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}, {'<', '>'}},
                {'{'}
            };
        }
    });

    registry.register_language({
        "ini",
        {"ini", "conf", "cfg", "desktop", "service", "editorconfig"},
        {".gitconfig", ".npmrc", ".editorconfig"},
        []() -> LanguageConfig {
            return {
                "ini",
                tree_sitter_ini,
                INI_QUERY,
                ";",
                {},
                {{'[', ']'}, {'"', '"'}},
                {}
            };
        }
    });
}
