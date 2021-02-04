/*
 * Copyright (c) 2020 YuQing <384681@qq.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <sys/stat.h>
#include <limits.h>
#include "fastcommon/shared_func.h"
#include "fastcommon/logger.h"
#include "client_global.h"
#include "client_func.h"
#include "client_proto.h"
#include "simple_connection_manager.h"

static inline int make_connection(FDIRClientContext *client_ctx,
        ConnectionInfo *conn)
{
    if (conn->sock >= 0) {
        return 0;
    }

    return conn_pool_connect_server(conn, client_ctx->connect_timeout);
}

static int check_realloc_group_servers(FDIRServerGroup *server_group)
{
    int bytes;
    int alloc_size;
    ConnectionInfo *servers;

    if (server_group->alloc_size > server_group->count) {
        return 0;
    }

    if (server_group->alloc_size > 0) {
        alloc_size = server_group->alloc_size * 2;
    } else {
        alloc_size = 4;
    }
    bytes = sizeof(ConnectionInfo) * alloc_size;
    servers = (ConnectionInfo *)fc_malloc(bytes);
    if (servers == NULL) {
        return errno != 0 ? errno : ENOMEM;
    }
    memset(servers, 0, bytes);

    if (server_group->count > 0) {
        memcpy(servers, server_group->servers,
                sizeof(ConnectionInfo) * server_group->count);
    }

    server_group->servers = servers;
    server_group->alloc_size = alloc_size;
    return 0;
}

static ConnectionInfo *get_spec_connection(FDIRClientContext *client_ctx,
        const ConnectionInfo *target, int *err_no)
{
    FDIRServerGroup *cluster_sarray;
    ConnectionInfo *conn;
    ConnectionInfo *end;

    cluster_sarray = (FDIRServerGroup *)client_ctx->conn_manager.args[0];
    end = cluster_sarray->servers + cluster_sarray->count;
    for (conn=cluster_sarray->servers; conn<end; conn++) {
        if (FC_CONNECTION_SERVER_EQUAL1(*conn, *target)) {
            break;
        }
    }

    if (conn == end) {
        if (check_realloc_group_servers(cluster_sarray) != 0) {
            *err_no = ENOMEM;
            return NULL;
        }

        conn = cluster_sarray->servers + cluster_sarray->count++;
        conn_pool_set_server_info(conn, target->ip_addr, target->port);
    }

    if ((*err_no=make_connection(client_ctx, conn)) != 0) {
        return NULL;
    }
    return conn;
}

static ConnectionInfo *get_connection(FDIRClientContext *client_ctx,
        int *err_no)
{
    int index;
    int i;
    ConnectionInfo *server;
    ConnectionInfo *conn;

    index = rand() % client_ctx->server_group.count;
    server = client_ctx->server_group.servers + index;
    if ((conn=get_spec_connection(client_ctx, server, err_no)) != NULL) {
        return conn;
    }

    i = (index + 1) % client_ctx->server_group.count;
    while (i != index) {
        server = client_ctx->server_group.servers + i;
        if ((conn=get_spec_connection(client_ctx, server, err_no)) != NULL) {
            return conn;
        }

        i = (i + 1) % client_ctx->server_group.count;
    }

    logError("file: "__FILE__", line: %d, "
            "get_connection fail, configured server count: %d",
            __LINE__, client_ctx->server_group.count);
    return NULL;
}

static ConnectionInfo *get_master_connection(FDIRClientContext *client_ctx,
        int *err_no)
{
    ConnectionInfo *conn; 
    FDIRClientServerEntry master;

    if (client_ctx->conn_manager.master_cache.conn != NULL) {
        return client_ctx->conn_manager.master_cache.conn;
    }

    do {
        if ((*err_no=fdir_client_get_master(client_ctx, &master)) != 0) {
            break;
        }

        if ((conn=get_spec_connection(client_ctx, &master.conn,
                        err_no)) == NULL)
        {
            break;
        }

        client_ctx->conn_manager.master_cache.conn = conn;
        return conn;
    } while (0);

    logError("file: "__FILE__", line: %d, "
            "get_master_connection fail, errno: %d",
            __LINE__, *err_no);
    return NULL;
}

static ConnectionInfo *get_readable_connection(
        FDIRClientContext *client_ctx, int *err_no)
{
    ConnectionInfo *conn; 
    FDIRClientServerEntry server;

    do {
        if ((*err_no=fdir_client_get_readable_server(
                        client_ctx, &server)) != 0)
        {
            break;
        }

        if ((conn=get_spec_connection(client_ctx, &server.conn,
                        err_no)) == NULL)
        {
            break;
        }

        return conn;
    } while (0);

    logError("file: "__FILE__", line: %d, "
            "get_readable_connection fail, errno: %d",
            __LINE__, *err_no);
        return NULL;
}

static void close_connection(FDIRClientContext *client_ctx,
        ConnectionInfo *conn)
{
    if (client_ctx->conn_manager.master_cache.conn == conn) {
        client_ctx->conn_manager.master_cache.conn = NULL;
    }

    conn_pool_disconnect_server(conn);
}

static const struct fdir_connection_parameters *get_connection_params(
        struct fdir_client_context *client_ctx, ConnectionInfo *conn)
{
    return NULL;
}

int fdir_simple_connection_manager_init(FDIRConnectionManager *conn_manager)
{
    FDIRServerGroup *cluster_sarray;
    int result;

    cluster_sarray = (FDIRServerGroup *)fc_malloc(sizeof(FDIRServerGroup));
    if (cluster_sarray == NULL) {
        return ENOMEM;
    }

    if ((result=fdir_alloc_group_servers(cluster_sarray, 4)) != 0) {
        return result;
    }

    conn_manager->args[0] = cluster_sarray;
    conn_manager->args[1] = NULL;
    conn_manager->get_connection = get_connection;
    conn_manager->get_spec_connection = get_spec_connection;
    conn_manager->get_master_connection = get_master_connection;
    conn_manager->get_readable_connection = get_readable_connection;

    conn_manager->release_connection = NULL;
    conn_manager->close_connection = close_connection;
    conn_manager->get_connection_params = get_connection_params;
    conn_manager->master_cache.conn = NULL;
    return 0;
}

void fdir_simple_connection_manager_destroy(FDIRConnectionManager *conn_manager)
{
    FDIRServerGroup *cluster_sarray;

    if (conn_manager->args[0] != NULL) {
        cluster_sarray = (FDIRServerGroup *)conn_manager->args[0];
        if (cluster_sarray->servers != NULL) {
            free(cluster_sarray->servers);
            cluster_sarray->servers = NULL;
            cluster_sarray->count = cluster_sarray->alloc_size = 0;
        }

        free(cluster_sarray);
        conn_manager->args[0] = NULL;
    }
}
