#include "common/memory/memory.h"
#include "common/math/math_types.h"
#include "common/math/math.h"
#include "common/math/quat.h"
#include "common/math/vector2.h"
#include "common/math/matrix33.h"
#include "common/math/matrix44.h"
#include "common/math/simd/simd.h"
#include "common/types.h"
#include "common/log/log.h"
#include "common/container/array.h"
#include "common/container/hash.h"
#include "common/container/queue.h"
#include "common/ecs/entitymanager.h"
#include "runtime/runtime.h"

#include "common/resource/resource_manager.h"
#include "common/resource/package_manager.h"


#include "resources/package.h"
#include "resources/lua.h"

#include "common/string/stringid_types.h"
#include <common/string/stringid.h>

#include "common/cvar/cvar.h"
#include "common/command_line/command_line.h"

#include "common/lua/lua_env.h"

#include "resources/lua.h"

#include "cvars/cvars.h"

#include <iostream>

#include "enet/enet.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

using namespace cetech;
using namespace rapidjson;



#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
namespace cetech {
    namespace console_server_globals {
        typedef void (* command_clb_t)(const rapidjson::Document&, rapidjson::Document&);
        
        struct ConsoleServer {
            int server_socket;
            int socket_max;
            fd_set socket_set;

            Array < int > client_socket;
            Array < sockaddr_in > client_addr;
            
            Hash<command_clb_t> cmds;

            ConsoleServer() : client_socket(memory_globals::default_allocator()), client_addr(
                                  memory_globals::default_allocator()), cmds(memory_globals::default_allocator()) {}

        };

        ConsoleServer* _cs;
    }

    namespace console_server_globals {
        void register_command(const char* name, const command_clb_t clb) {
            hash::set(_cs->cmds, stringid64::from_cstring(name), clb);
        }
        
        void init() {
            _cs = new ConsoleServer();

            if ((_cs->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                log::error("console_server", "Could not create socket");
                return;
            }

            
            const int oldFlag = fcntl(_cs->server_socket, F_GETFL, 0);
            if (fcntl(_cs->server_socket, F_SETFL, oldFlag | O_NONBLOCK) == -1) {
                log::error("console_server", "Could not set nonblocking mode");
                return;
            }
            
            sockaddr_in socket_addr;
            socket_addr.sin_family = AF_INET;
            socket_addr.sin_port = htons(cvars::console_server_port.value_i);
            socket_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(_cs->server_socket, (sockaddr*)&socket_addr, sizeof(socket_addr)) == -1) {
                log::error("console_server", "Could not bind socket");
                return;
            }

            if (listen(_cs->server_socket, 10) == -1) {
                log::error("console_server", "Could not listen.");
                return;
            }

            _cs->socket_max = _cs->server_socket;
            
            FD_ZERO(&(_cs->socket_set));
            FD_SET(_cs->server_socket, &(_cs->socket_set));
        }

        void parse_packet(uint32_t client, const char* packet, const uint32_t size){
            log::info("client_server", "Client msg: %s", packet);
            
            rapidjson::Document document;
            document.Parse(packet);

            if (document.HasParseError()) {
                log::error("console_server", "Packet parse error: %s", GetParseError_En(document.GetParseError()), document.GetErrorOffset());
                return;
            }
            
            if( !document.HasMember("name") ) {
                log::error("console_server", "packer require key \"name\"");
                return;
            }
            
            command_clb_t cmd = hash::get<command_clb_t>(_cs->cmds, stringid64::from_cstring(document["name"].GetString()), nullptr);
            if( cmd == nullptr ) {
                log::error("console_server", "Command \"%s\" not found.", document["name"].GetString());
                return;
            }
            
            rapidjson::Document document_out;
            cmd(document, document_out);
            
            if( document_out.IsObject() ) {
            }
        }
        
        void tick() {
            if (select(_cs->socket_max + 1, &(_cs->socket_set), NULL, NULL, NULL) != 0) {
                if (FD_ISSET(_cs->server_socket, &(_cs->socket_set))) {
                    const uint32_t client_id = array::size(_cs->client_socket);

                    sockaddr_in client_ifo;
                    socklen_t addrlen = sizeof(client_ifo);

                    int cl_socket = accept(_cs->server_socket, (sockaddr*)&client_ifo, &addrlen);

                    log::info("console_server", "Client connected.");
                    
                    array::push_back(_cs->client_socket, cl_socket);
                    array::push_back(_cs->client_addr, client_ifo);
                }

                const uint32_t client_len = array::size(_cs->client_socket);
                for (uint32_t i = 0; i < client_len; ++i) {
                    if(_cs->client_socket[i] == 0 ) {
                        continue;
                    }
                    
                    if (FD_ISSET(_cs->client_socket[i], &(_cs->socket_set))) {
                        char buffer[4096] = {0};
                        int lenght = 0;

                        if ((lenght = recv(_cs->client_socket[i], (void*)buffer, 4096 - 1, 0)) <= 0) {
                            close(_cs->client_socket[i]);
                            log::info("console_server", "Client disconnected.");

                            _cs->client_socket[i] = 0;

                        } else {
                            buffer[lenght] = 0;
                            
                            parse_packet(i, buffer, lenght);
                        }
                    }
                }

                FD_ZERO(&(_cs->socket_set));
                FD_SET(_cs->server_socket, &(_cs->socket_set));
                for (uint32_t i = 0; i < client_len; ++i) {
                    if(_cs->client_socket[i] == 0 ) {
                        continue;
                    }
                    
                    if( _cs->client_socket[i] > _cs->socket_max ) {
                         _cs->socket_max = _cs->client_socket[i];
                    }
                    
                    FD_SET(_cs->client_socket[i], &(_cs->socket_set));
                }

                if( _cs->socket_max < _cs->server_socket ) {
                    _cs->socket_max = _cs->server_socket;
                }
                
            }
        }
    }
}


void cmd_lua_execute(const rapidjson::Document& in, rapidjson::Document& out) {
    lua_enviroment::execute_string(lua_enviroment_globals::global_env(), in["args"]["script"].GetString());
}

void frame_start() {
    runtime::frame_start();
    console_server_globals::tick();
    console_server_globals::register_command("lua.execute", &cmd_lua_execute);
}

void frame_end() {
    runtime::frame_end();
}

void frame_body() {
    //log::info("frame", "frame");
}

void run() {
    while (1) {
        frame_start();
        frame_body();
        frame_end();
    }
}

void join_build_dir(char* buffer, size_t max_len, const char* basename) {
    memset(buffer, 0, max_len);

    size_t len = strlen(cvars::rm_build_dir.value_str);
    memcpy(buffer, cvars::rm_build_dir.value_str, len);

    strcpy(buffer + len, basename);
}

void load_config_json() {
    char config_path[1024] = {0};
    join_build_dir(config_path, 1024, "config.json");

    File f = runtime::file::from_file(config_path, "rb");
    const uint64_t f_sz = runtime::file::size(f);
    void* mem = memory_globals::default_allocator().allocate(f_sz + 1);
    memset(mem, 0, f_sz + 1);

    runtime::file::read(f, mem, sizeof(char), f_sz);

    rapidjson::Document document;
    document.Parse((const char*)mem);
    cvar::load_from_json(document);

    memory_globals::default_allocator().deallocate(mem);

    runtime::file::close(f);
}

void make_path(char* buffer, size_t max_size, const char* path) {
    memset(buffer, 0, max_size);
    strcpy(buffer, path);

    const size_t len = strlen(buffer);
    if (buffer[len - 1] != '/') {
        buffer[len] = '/';
    }
}

void parse_command_line() {
    char buffer[1024] = {0};

    const char* source_dir = command_line::get_parameter("source-dir", 'i');
    const char* build_dir = command_line::get_parameter("build-dir", 'd');

    if (source_dir) {
        make_path(buffer, 1024, source_dir);
        cvar_internal::force_set(cvars::rm_source_dir, buffer);
    }

    if (build_dir) {
        make_path(buffer, 1024, build_dir);
        cvar_internal::force_set(cvars::rm_build_dir, buffer);
    }
}

void init_boot() {
    StringId64_t boot_pkg_name_h = murmur_hash_64(cvars::boot_pkg.value_str, strlen(cvars::boot_pkg.value_str), 22);
    StringId64_t boot_script_name_h =
        murmur_hash_64(cvars::boot_script.value_str, strlen(cvars::boot_script.value_str), 22);

    resource_manager::load(package_manager::type_name(), boot_pkg_name_h);
    package_manager::load(boot_pkg_name_h);

    StringId64_t lua_hash = murmur_hash_64("lua", 3, 22);

    const resource_lua::Resource* res_lua = (const resource_lua::Resource*)resource_manager::get(lua_hash,
                                                                                                 boot_script_name_h);
    lua_enviroment::execute_resource(lua_enviroment_globals::global_env(), res_lua);
}

void compile_all_resource() {
    char* files[4096] = {0};
    uint32_t files_count = 0;
    const size_t source_dir_len = strlen(cvars::rm_source_dir.value_str);

    runtime::dir::listdir(cvars::rm_source_dir.value_str, "", files, &files_count);

    for (uint32_t i = 0; i < files_count; ++i) {
        const char* path_base = files[i] + source_dir_len; /* Base path */

        resource_manager::compile(path_base);
    }
}

struct ResourceRegistration {
    StringId64_t type;

    resource_manager::resource_compiler_clb_t compiler;
    resource_manager::resource_loader_clb_t loader;
    resource_manager::resource_unloader_clb_t unloader;
};

void register_resources(const ResourceRegistration* regs) {
    const ResourceRegistration* it = regs;

    while (it->type != 0) {
        resource_manager::register_unloader(it->type, it->unloader);
        resource_manager::register_loader(it->type, it->loader);
        resource_manager::register_compiler(it->type, it->compiler);
        ++it;
    }
}

void init() {
    memory_globals::init();

    parse_command_line();

    runtime::init();

    resource_package::init();
    resource_lua::init();

    resource_manager_globals::init();
    package_manager_globals::init();

    load_config_json();

    console_server_globals::init();

    ResourceRegistration resource_regs[] = {
        {resource_package::type_hash(), & resource_package::compiler, & resource_package::loader,
         & resource_package::unloader},
        {resource_lua::type_hash(), & resource_lua::compiler, & resource_lua::loader, & resource_lua::unloader},
        {0, nullptr, nullptr, nullptr}
    };
    register_resources(resource_regs);

    if (command_line::has_argument("compile", 'c')) {
        compile_all_resource();
    }

    init_boot();

    cvar::dump_all();

    //     Window w = runtime::window::make_window(
    //         "aaa",
    //         runtime::window::WINDOWPOS_CENTERED, runtime::window::WINDOWPOS_CENTERED,
    //         800, 600,
    //         runtime::window::WINDOW_NOFLAG
    //         );
}

void shutdown() {
    runtime::shutdown();
    memory_globals::shutdown();
    resource_manager_globals::shutdown();
    package_manager_globals::shutdown();
}

int main(int argc, const char** argv) {
    command_line::set_args(argc, argv);

    init();
    run();
    shutdown();

    return 0; // TODO: error check
}
