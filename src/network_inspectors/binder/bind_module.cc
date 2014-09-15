/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

// bind_module.cc author Russ Combs <rucombs@cisco.com>

#include "bind_module.h"

#include <assert.h>
#include <string.h>

#include <string>
using namespace std;

#include "binding.h"
#include "protocols/packet.h"
#include "parser/parse_ip.h"

THREAD_LOCAL BindStats bstats;

static const char* bind_pegs[] =
{
    "packets",
    "blocks",
    "allows",
    "inspects",
    nullptr
};

//-------------------------------------------------------------------------
// binder module
//-------------------------------------------------------------------------

static const Parameter binder_when_params[] =
{
    // FIXIT when.policy_id should be an arbitrary string auto converted
    // into index for binder matching and lookups
    { "policy_id", Parameter::PT_INT, "0:", nullptr,
      "unique ID for selection of this config by external logic" },

    { "vlans", Parameter::PT_BIT_LIST, "4095", nullptr,
      "list of VLAN IDs" },

    { "nets", Parameter::PT_ADDR_LIST, nullptr, nullptr,
      "list of networks" },

    { "proto", Parameter::PT_ENUM, "any | ip | icmp | tcp | udp", nullptr,
      "protocol" },

    { "ports", Parameter::PT_BIT_LIST, "65535", nullptr,
      "list of ports" },

    { "role", Parameter::PT_ENUM, "client | server | any", "any",
      "use the given configuration on one or any end of a session" },

    { "service", Parameter::PT_STRING, nullptr, nullptr,
      "override default configuration" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

static const Parameter binder_use_params[] =
{
    { "action", Parameter::PT_ENUM, "block | allow | inspect", "inspect",
      "what to do with matching traffic" },

    { "file", Parameter::PT_STRING, nullptr, nullptr,
      "use configuration in given file" },

    { "service", Parameter::PT_STRING, nullptr, nullptr,
      "override automatic service identification" },

    { "type", Parameter::PT_STRING, nullptr, nullptr,
      "select module for binding" },

    { "name", Parameter::PT_STRING, nullptr, "defaults to type",
      "symbol name" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

static const Parameter s_params[] =
{
    { "when", Parameter::PT_TABLE, binder_when_params, nullptr,
      "match criteria" },

    { "use", Parameter::PT_TABLE, binder_use_params, nullptr,
      "target configuration" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

BinderModule::BinderModule() : Module(BIND_NAME, BIND_HELP, s_params)
{ work = nullptr; }

BinderModule::~BinderModule()
{
    if ( work )
        delete work;
}

ProfileStats* BinderModule::get_profile() const
{ return &bindPerfStats; }

bool BinderModule::set(const char* fqn, Value& v, SnortConfig*)
{
    // both
    if ( !strcmp(fqn, "binder.when.service") )
        work->when.svc = v.get_string();

    else if ( !strcmp(fqn, "binder.use.service") )
        work->use.svc = v.get_string();

    // when
    else if ( v.is("policy_id") )
        work->when.id = v.get_long();

    else if ( v.is("nets") )
        work->when.nets = sfip_var_from_string(v.get_string());

    else if ( v.is("proto") )
    {
        const unsigned mask[] =
        { 
            PROTO_BIT__ALL, PROTO_BIT__IP, PROTO_BIT__ICMP, PROTO_BIT__TCP, PROTO_BIT__UDP
        };
        work->when.protos = mask[v.get_long()];
    }
    else if ( v.is("ports") )
        v.get_bits(work->when.ports);

    else if ( v.is("role") )
        work->when.role = (BindRole)v.get_long();

    else if ( v.is("vlans") )
        v.get_bits(work->when.vlans);

    // use
    else if ( v.is("action") )
        work->use.action = (BindAction)(v.get_long() + 1);

    else if ( v.is("file") )
        work->use.file = v.get_string();

    else if ( v.is("name") )
        work->use.name = v.get_string();

    else if ( v.is("type") )
        work->use.type = v.get_string();

    else
        return false;

    return true;
}

bool BinderModule::begin(const char* fqn, int idx, SnortConfig*)
{
    if ( idx && !strcmp(fqn, BIND_NAME) )
        work = new Binding;

    return true;
}

bool BinderModule::end(const char* fqn, int idx, SnortConfig*)
{
    if ( idx && !strcmp(fqn, BIND_NAME) )
    {
        bindings.push_back(work);
        work = nullptr;
    }
    return true;
}

void BinderModule::add(const char* svc, const char* type)
{
    Binding* b = new Binding;
    b->when.svc = svc;
    b->use.type = type;
    bindings.push_back(b);
}

void BinderModule::add(unsigned proto, const char* type)
{
    Binding* b = new Binding;
    b->when.protos = proto;
    b->use.type = type;
    bindings.push_back(b);
}

vector<Binding*> BinderModule::get_data()
{
    return bindings;  // move semantics
}

const char** BinderModule::get_pegs() const
{ return bind_pegs; }

PegCount* BinderModule::get_counts() const
{ return (PegCount*)&bstats; }

