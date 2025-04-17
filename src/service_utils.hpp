#pragma once

template <typename T>
void load_services(auto config, auto key_base)
{
    config->scan_level(key_base, [config, key_base](auto const &subkey)
                       {
        auto login = std::make_shared<T>();
        login->load_from([config, key_base, subkey](std::string_view k){ return config->get(std::format("{}{}.{}", key_base, subkey,k)); });
        registrar::add<T>(subkey, login); });
}

template <typename T>
void save_services(auto config, auto key_base)
{
    registrar::all<T>([config, key_base](std::string const &key, auto login)
                      { login->save_to([config, key, key_base](std::string_view k, std::string v)
                                       { config->set(std::format("{}{}.{}", key_base, key, k), v); }); });
}

