#include <llarp/bencode.h>
#include <llarp/router_contact.h>
#include <llarp/version.h>

#include "buffer.hpp"
#include "logger.hpp"

extern "C" {

void
llarp_rc_free(struct llarp_rc *rc)
{
  if(rc->exits)
    llarp_xi_list_free(rc->exits);
  if(rc->addrs)
    llarp_ai_list_free(rc->addrs);

  rc->exits = 0;
  rc->addrs = 0;
}

struct llarp_rc_decoder
{
  struct llarp_rc *rc;
  struct llarp_alloc *mem;
};

static bool
llarp_rc_decode_dict(struct dict_reader *r, llarp_buffer_t *key)
{
  uint64_t v;
  llarp_buffer_t strbuf;
  llarp_rc *rc = static_cast< llarp_rc * >(r->user);

  if(!key)
    return true;

  if(llarp_buffer_eq(*key, "a"))
  {
    if(rc->addrs)
    {
      llarp_ai_list_free(rc->addrs);
    }
    rc->addrs = llarp_ai_list_new();
    return llarp_ai_list_bdecode(rc->addrs, r->buffer);
  }

  if(llarp_buffer_eq(*key, "k"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz != sizeof(llarp_pubkey_t))
      return false;
    memcpy(rc->pubkey, strbuf.base, sizeof(llarp_pubkey_t));
    return true;
  }

  if(llarp_buffer_eq(*key, "u"))
  {
    if(!bencode_read_integer(r->buffer, &rc->last_updated))
      return false;
    return true;
  }

  if(llarp_buffer_eq(*key, "v"))
  {
    if(!bencode_read_integer(r->buffer, &v))
      return false;
    return v == LLARP_PROTO_VERSION;
  }

  if(llarp_buffer_eq(*key, "x"))
  {
    if(rc->exits)
    {
      llarp_xi_list_free(rc->exits);
    }
    rc->exits = llarp_xi_list_new();
    return llarp_xi_list_bdecode(rc->exits, r->buffer);
  }

  if(llarp_buffer_eq(*key, "z"))
  {
    if(!bencode_read_string(r->buffer, &strbuf))
      return false;
    if(strbuf.sz != sizeof(llarp_sig_t))
      return false;
    memcpy(rc->signature, strbuf.base, sizeof(llarp_sig_t));
    return true;
  }

  return false;
}

void
llarp_rc_copy(struct llarp_rc *dst, const struct llarp_rc *src)
{
  llarp_rc_free(dst);
  llarp_rc_clear(dst);
  memcpy(dst->pubkey, src->pubkey, sizeof(llarp_pubkey_t));
  memcpy(dst->signature, src->signature, sizeof(llarp_sig_t));
  dst->last_updated = src->last_updated;

  if(src->addrs)
  {
    dst->addrs = llarp_ai_list_new();
    llarp_ai_list_copy(dst->addrs, src->addrs);
  }
  if(src->exits)
  {
    dst->exits = llarp_xi_list_new();
    llarp_xi_list_copy(dst->exits, src->exits);
  }
}

bool
llarp_rc_bdecode(struct llarp_rc *rc, llarp_buffer_t *buff)
{
  dict_reader r = {buff, rc, &llarp_rc_decode_dict};
  return bencode_read_dict(buff, &r);
}

bool
llarp_rc_verify_sig(struct llarp_crypto *crypto, struct llarp_rc *rc)
{
  // maybe we should copy rc before modifying it
  // would that make it more thread safe?
  // jeff agrees
  bool result = false;
  llarp_sig_t sig;
  byte_t tmp[MAX_RC_SIZE];

  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  // copy sig
  memcpy(sig, rc->signature, sizeof(llarp_sig_t));
  // zero sig
  size_t sz = 0;
  while(sz < sizeof(llarp_sig_t))
    rc->signature[sz++] = 0;

  // bencode
  if(llarp_rc_bencode(rc, &buf))
  {
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    result  = crypto->verify(rc->pubkey, buf, sig);
  }
  else
    llarp::Warn(__FILE__, "RC encode failed");
  // restore sig
  memcpy(rc->signature, sig, sizeof(llarp_sig_t));
  return result;
}

bool
llarp_rc_bencode(struct llarp_rc *rc, llarp_buffer_t *buff)
{
  /* write dict begin */
  if(!bencode_start_dict(buff))
    return false;

  if(rc->addrs)
  {
    /* write ai if they exist */
    if(!bencode_write_bytestring(buff, "a", 1))
      return false;
    if(!llarp_ai_list_bencode(rc->addrs, buff))
      return false;
  }
  /* write pubkey */
  if(!bencode_write_bytestring(buff, "k", 1))
    return false;
  if(!bencode_write_bytestring(buff, rc->pubkey, sizeof(llarp_pubkey_t)))
    return false;

  /* write last updated */
  if(!bencode_write_bytestring(buff, "u", 1))
    return false;
  if(!bencode_write_uint64(buff, rc->last_updated))
    return false;

  /* write version */
  if(!bencode_write_version_entry(buff))
    return false;

  if(rc->exits)
  {
    /* write ai if they exist */
    if(!bencode_write_bytestring(buff, "x", 1))
      return false;
    if(!llarp_xi_list_bencode(rc->exits, buff))
      return false;
  }

  /* write signature */
  if(!bencode_write_bytestring(buff, "z", 1))
    return false;
  if(!bencode_write_bytestring(buff, rc->signature, sizeof(llarp_sig_t)))
    return false;
  return bencode_end(buff);
}
}