// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*- 
// vim: ts=8 sw=2 sts=2 expandtab

/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */
#include <ctype.h>
#include <sstream>
#include "ObjectStore.h"
#include "common/Formatter.h"
#include "common/safe_io.h"

#include "memstore/MemStore.h"
#if defined(WITH_BLUESTORE)
#include "bluestore/BlueStore.h"
#endif

using std::string;

std::unique_ptr<ObjectStore> ObjectStore::create(
  CephContext *cct,
  const string& type,
  const string& data)
{
  if (type == "memstore") {
    return std::make_unique<MemStore>(cct, data);
  }
#if defined(WITH_BLUESTORE)
  if (type == "bluestore" || type == "random") {
    return std::make_unique<BlueStore>(cct, data);
  }
#endif
  return nullptr;
}

std::unique_ptr<ObjectStore> ObjectStore::create(
  CephContext *cct,
  const string& type,
  const string& data,
  const string& journal,
  osflagbits_t flags)
{
  if (type == "filestore") {
    lgeneric_derr(cct) << __func__ << ": FileStore has been deprecated and is no longer supported" << dendl;
    return nullptr;
  }
  return create(cct, type, data);
}


int ObjectStore::probe_block_device_fsid(
  CephContext *cct,
  const string& path,
  uuid_d *fsid)
{
#if defined(WITH_BLUESTORE)
  // first try bluestore -- it has a crc on its header and will fail
  // reliably.
  int r = BlueStore::get_block_device_fsid(cct, path, fsid);
  if (r == 0) {
    lgeneric_dout(cct, 0) << __func__ << " " << path << " is bluestore, "
			  << *fsid << dendl;
    return r;
  } else {
    return -EINVAL;
  }
#else
  return -EINVAL;
#endif
}

int ObjectStore::write_meta(const std::string& key,
			    const std::string& value)
{
  string v = value;
  v += "\n";
  int r = safe_write_file(path.c_str(), key.c_str(),
			  v.c_str(), v.length(), 0600);
  if (r < 0)
    return r;
  return 0;
}

int ObjectStore::read_meta(const std::string& key,
			   std::string *value)
{
  char buf[4096];
  int r = safe_read_file(path.c_str(), key.c_str(),
			 buf, sizeof(buf));
  if (r <= 0)
    return r;
  // drop trailing newlines
  while (r && isspace(buf[r-1])) {
    --r;
  }
  *value = string(buf, r);
  return 0;
}

int ObjectStore::get_ideal_list_max()
{
  return cct->_conf->osd_objectstore_ideal_list_max;
}


//store_fsck_stats_t

std::ostream& operator<<(std::ostream& out, const store_fsck_stats_t& s)
{
  out << "(" << s.num_objects << " objects, "
      << s.num_sharded_objects << " sharded objects, "
      << s.num_extents << " extents, "
      << s.num_blobs << " blobs, "
      << s.num_spanning_blobs << " spanning blobs, "
      << s.num_shared_blobs << " shared blobs, "
      << s.warnings_found << " warnings found, "
      << s.errors_found << " errors found, "
      << s.repaired_found << " repaired found"
      << ")";
  return out;
}

void store_fsck_stats_t::dump(Formatter* f) const
{
  f->dump_unsigned("num_objects", num_objects);
  f->dump_unsigned("num_sharded_objects", num_sharded_objects);
  f->dump_unsigned("num_extents", num_extents);
  f->dump_unsigned("num_blobs", num_blobs);
  f->dump_unsigned("num_spanning_blobs", num_spanning_blobs);
  f->dump_unsigned("num_shared_blobs", num_shared_blobs);
  f->dump_unsigned("warnings_found", warnings_found);
  f->dump_unsigned("errors_found", errors_found);
  f->dump_unsigned("repaired_found", repaired_found);
}