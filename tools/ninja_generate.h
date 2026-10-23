#pragma once

#include "nob.h"

// ================ String and command helpers ======================
// Own the pointer array; strings themselves live in Nob's temporary arena.
struct Strings : Nob_Cmd
{
    Strings() : Nob_Cmd{}
    {
    }
    template <typename... T> Strings(T... values) : Strings()
    {
        const char* initial[] = {values...};
        nob_da_append_many(this, initial, sizeof...(values));
    }
    Strings(const Strings& other) : Strings()
    {
        nob_cmd_extend(this, &other);
    }
    Strings& operator=(const Strings& other)
    {
        if (this != &other)
        {
            count = 0;
            nob_cmd_extend(this, &other);
        }
        return *this;
    }
    ~Strings()
    {
        nob_da_free(*this);
    }
    void add(const char* value)
    {
        nob_da_append(this, value);
    }
    const char* const* begin() const
    {
        return items;
    }
    const char* const* end() const
    {
        return count ? items + count : items;
    }
};

static bool equal(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}
static bool endsWith(const char* text, const char* suffix)
{
    return nob_sv_end_with(nob_sv_from_cstr(text), suffix);
}
static void append(Strings& dst, const Strings& src)
{
    nob_cmd_extend(&dst, &src);
}
static Strings words(const char* text)
{
    Strings result;
    while (*text)
    {
        while (isspace((unsigned char)*text))
        {
            ++text;
        }
        const char* start = text;
        while (*text && !isspace((unsigned char)*text))
        {
            ++text;
        }
        if (text != start)
        {
            result.add(nob_temp_sv_to_cstr(nob_sv_from_parts(start, text - start)));
        }
    }
    return result;
}
static const char* render(const Strings& args)
{
    Nob_String_Builder text = {};
    nob__win32_cmd_quote(args, &text);
    nob_sb_append_null(&text);
    const char* result = nob_temp_strdup(text.items);
    nob_sb_free(text);
    return result;
}
static bool run(const Strings& args)
{
    Nob_Cmd cmd = args;
    return nob_cmd_run(&cmd);
}
// ================ File and path helpers ======================
static bool write(const char* path, const void* data, size_t size)
{
    Nob_String_Builder old = {};
    if (nob_file_exists(path))
    {
        if (!nob_read_entire_file(path, &old))
        {
            return false;
        }
        bool same = old.count == size && (!size || memcmp(old.items, data, size) == 0);
        nob_sb_free(old);
        if (same)
        {
            return true;
        }
    }
    return nob_write_entire_file(path, data, size);
}
static bool write(const char* path, const char* text)
{
    return write(path, text, strlen(text));
}
static bool write(const char* path, const Nob_String_Builder& text)
{
    return write(path, text.items, text.count);
}
static Strings lines(const char* path)
{
    Strings result;
    Nob_String_Builder text = {};
    if (nob_file_exists(path) && nob_read_entire_file(path, &text))
    {
        auto view = nob_sv_from_parts(text.items, text.count);
        while (view.count)
        {
            auto line = nob_sv_chop_by_delim(&view, '\n');
            if (line.count && line.data[line.count - 1] == '\r')
            {
                --line.count;
            }
            if (line.count)
            {
                result.add(nob_temp_sv_to_cstr(line));
            }
        }
    }
    nob_sb_free(text);
    return result;
}
static const char* fullPath(const char* path)
{
    // Nob has no path canonicalization API. The builder currently targets Windows.
    DWORD size = GetFullPathNameA(path, 0, nullptr, nullptr);
    NOB_ASSERT(size);
    char* result = (char*)nob_temp_alloc(size);
    NOB_ASSERT(GetFullPathNameA(path, size, result, nullptr));
    for (char* c = result; *c; ++c)
    {
        if (*c == '\\')
        {
            *c = '/';
        }
    }
    return result;
}
static const char* relativePath(const char* path, const char* root)
{
    size_t size = strlen(root);
    return strncmp(path, root, size) == 0 && path[size] == '/' ? path + size + 1 : path;
}
static const char* windowsPath(const char* path)
{
    char* result = nob_temp_strdup(path);
    for (char* c = result; *c; ++c)
    {
        if (*c == '/')
        {
            *c = '\\';
        }
    }
    return result;
}
static bool makeDirectory(const char* path)
{
    if (nob_file_exists(path))
    {
        return true;
    }
    const char* slash = strrchr(path, '/');
    if (slash && slash > path + 2 && !makeDirectory(nob_temp_sv_to_cstr(nob_sv_from_parts(path, slash - path))))
    {
        return false;
    }
    return nob_mkdir_if_not_exists(path);
}
static bool removeFile(const char* path)
{
    return !nob_file_exists(path) || nob_delete_file(path);
}
static uint64_t fileTime(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
    {
        return 0;
    }
    return (uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}
static int comparePaths(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}
static bool collectFiles(const char* directory, Strings& files, const Strings& skip = {})
{
    Nob_File_Paths entries = {};
    if (!nob_read_entire_dir(directory, &entries))
    {
        return false;
    }
    bool success = true;
    for (size_t i = 0; success && i < entries.count; ++i)
    {
        const char* name = entries.items[i];
        if (name[0] == '.')
        {
            continue;
        }
        const char* path = nob_temp_sprintf("%s/%s", directory, name);
        Nob_File_Type type = nob_get_file_type(path);
        if (type == NOB_FILE_DIRECTORY)
        {
            bool excluded = false;
            for (const char* folder : skip)
            {
                if (equal(name, folder))
                {
                    excluded = true;
                }
            }
            if (!excluded)
            {
                success = collectFiles(path, files, skip);
            }
        }
        else if (type == NOB_FILE_REGULAR)
        {
            files.add(path);
        }
        else if ((int)type < 0)
        {
            success = false;
        }
    }
    nob_da_free(entries);
    if (files.count > 1)
    {
        qsort(files.items, files.count, sizeof(*files.items), comparePaths);
    }
    return success;
}

// ================ Ninja generation ======================
struct Ninja
{
    Nob_String_Builder text = {};
    const char* compiler;
    const char* root;
    bool valid = true;

    Ninja(const char* compiler, const char* root) : compiler(compiler), root(root)
    {
        const char* rules =
            "# Generated by me_build.cpp; run build.bat to refresh sources/configuration.\n"
            "ninja_required_version = 1.10\nbuilddir = build\n"
            "rule cc\n  command = $compiler @\"$rsp\"\n  rspfile = $rsp\n  rspfile_content = $args\n"
            "  depfile = $depfile\n  deps = gcc\n  description = CXX $label\n"
            "rule link\n  command = $compiler @\"$rsp\"\n  rspfile = $rsp\n  rspfile_content = $args\n"
            "  restat = 1\n  description = LINK $label\n"
            "rule run\n  command = $cmd\n  restat = 1\n  description = $label\n"
            "rule copy\n  command = cmd /d /c copy /y \"$source\" \"$dest\" >nul\n  description = COPY $out\n\n";
        nob_sb_append_cstr(&text, rules);
    }
    Ninja(const Ninja&) = delete;
    Ninja& operator=(const Ninja&) = delete;
    ~Ninja()
    {
        nob_sb_free(text);
    }
    void escaped(const char* value, bool path = false)
    {
        for (; *value; ++value)
        {
            if (*value == '$' || (path && (*value == ' ' || *value == ':')))
            {
                nob_da_append(&text, '$');
            }
            nob_da_append(&text, *value);
        }
    }
    void paths(const Strings& paths)
    {
        for (const char* path : paths)
        {
            nob_da_append(&text, ' ');
            escaped(path, true);
        }
    }
    void edge(const char* rule, const Strings& outputs, const Strings& inputs, const Strings& order = {})
    {
        nob_sb_append_cstr(&text, "build");
        paths(outputs);
        nob_sb_appendf(&text, ": %s", rule);
        paths(inputs);
        if (order.count)
        {
            nob_sb_append_cstr(&text, " ||");
            paths(order);
        }
        nob_da_append(&text, '\n');
    }
    void variable(const char* name, const char* value)
    {
        nob_sb_appendf(&text, "  %s = ", name);
        escaped(value);
        nob_da_append(&text, '\n');
    }
    void ccCommand(const Strings& args, const char* output)
    {
        variable("compiler", render({compiler}));
        variable("rsp", nob_temp_sprintf("%s.rsp", output));
        variable("args", render(args));
        variable("label", relativePath(output, root));
    }
    const char* compile(const char* source, Strings flags, const char* group, const Strings& order = {})
    {
        const char* output = fullPath(nob_temp_sprintf("build/obj/%s/%s.o", group, relativePath(source, root)));
        const char* slash = strrchr(output, '/');
        valid = makeDirectory(nob_temp_sv_to_cstr(nob_sv_from_parts(output, slash - output))) && valid;
        const char* depfile = nob_temp_sprintf("%s.d", output);
        append(flags, {"-c", source, "-o", output, "-MMD", "-MF", depfile, "-MT", output});
        Strings inputs = {source, compiler};
        for (size_t i = 0; i + 1 < flags.count; ++i)
        {
            if (equal(flags.items[i], "-include-pch"))
            {
                inputs.add(flags.items[i + 1]);
            }
        }
        edge("cc", {output}, inputs, order);
        ccCommand(flags, output);
        variable("depfile", depfile);
        return output;
    }
    void link(const char* output, Strings flags, const Strings& objects, const Strings& extraOutputs = {})
    {
        Strings outputs = {output}, inputs = objects;
        append(outputs, extraOutputs);
        for (const char* arg : flags)
        {
            if (endsWith(arg, ".lib"))
            {
                inputs.add(arg);
            }
        }
        inputs.add(compiler);
        append(flags, objects);
        append(flags, {"-o", output});
        edge("link", outputs, inputs);
        ccCommand(flags, output);
    }
    void copy(const char* source, const char* dest)
    {
        edge("copy", {fullPath(dest)}, {fullPath(source)});
        variable("source", windowsPath(fullPath(source)));
        variable("dest", windowsPath(fullPath(dest)));
    }
    void defaults(const Strings& outputs)
    {
        nob_sb_append_cstr(&text, "default");
        paths(outputs);
        nob_da_append(&text, '\n');
    }
    bool save(const char* path) const
    {
        return valid && write(path, text);
    }
};

// ================ Ninja invocation ======================
static bool runNinja(const char* executable, const char* file, const char* target = nullptr)
{
    Strings args = {executable, "-k", "1", "-f", file};
    if (target)
    {
        args.add(target);
    }
    return run(args);
}
