/*
  Memcached library

  memcached_set()
  memcached_replace()
  memcached_add()

*/

#include "common.h"

static memcached_return memcached_send(memcached_st *ptr, 
                                       char *key, size_t key_length, 
                                       char *value, size_t value_length, 
                                       time_t expiration,
                                       uint16_t  flags,
                                       char *verb)
{
  size_t write_length;
  ssize_t sent_length;
  memcached_return rc;
  char buffer[MEMCACHED_DEFAULT_COMMAND_SIZE];
  unsigned int server_key;

  assert(value);
  assert(value_length);

  memset(buffer, 0, MEMCACHED_DEFAULT_COMMAND_SIZE);

  rc= memcached_connect(ptr);
  if (rc != MEMCACHED_SUCCESS)
    return rc;

  server_key= memcached_generate_hash(key, key_length) % ptr->number_of_hosts;

  write_length= snprintf(buffer, MEMCACHED_DEFAULT_COMMAND_SIZE, 
                        "%s %.*s %x %llu %zu\r\n", verb,
                        (int)key_length, key, flags, 
                        (unsigned long long)expiration, value_length);
  if (write_length >= MEMCACHED_DEFAULT_COMMAND_SIZE)
    return MEMCACHED_WRITE_FAILURE;
  if ((sent_length= send(ptr->hosts[server_key].fd, buffer, write_length, 0)) == -1)
    return MEMCACHED_WRITE_FAILURE;
  assert(write_length == sent_length);

  if ((sent_length= send(ptr->hosts[server_key].fd, value, value_length, 0)) == -1)
    return MEMCACHED_WRITE_FAILURE;
  assert(value_length == sent_length);

  if ((sent_length= send(ptr->hosts[server_key].fd, "\r\n", 2, 0)) == -1)
    return MEMCACHED_WRITE_FAILURE;
  assert(2 == sent_length);

  sent_length= recv(ptr->hosts[server_key].fd, buffer, MEMCACHED_DEFAULT_COMMAND_SIZE, 0);

  if (sent_length && buffer[0] == 'S')  /* STORED */
    return MEMCACHED_SUCCESS;
  else if (write_length && buffer[0] == 'N')  /* NOT_STORED */
    return MEMCACHED_NOTSTORED;
  else
    return MEMCACHED_READ_FAILURE;
}

memcached_return memcached_set(memcached_st *ptr, char *key, size_t key_length, 
                               char *value, size_t value_length, 
                               time_t expiration,
                               uint16_t  flags)
{
  memcached_return rc;
  LIBMEMCACHED_MEMCACHED_SET_START();
  rc= memcached_send(ptr, key, key_length, value, value_length,
                         expiration, flags, "set");
  LIBMEMCACHED_MEMCACHED_SET_END();
  return rc;
}

memcached_return memcached_add(memcached_st *ptr, char *key, size_t key_length,
                               char *value, size_t value_length, 
                               time_t expiration,
                               uint16_t  flags)
{
  memcached_return rc;
  LIBMEMCACHED_MEMCACHED_ADD_START();
  rc= memcached_send(ptr, key, key_length, value, value_length,
                         expiration, flags, "add");
  LIBMEMCACHED_MEMCACHED_ADD_END();
  return rc;
}

memcached_return memcached_replace(memcached_st *ptr, char *key, size_t key_length,
                                   char *value, size_t value_length, 
                                   time_t expiration,
                                   uint16_t  flags)
{
  memcached_return rc;
  LIBMEMCACHED_MEMCACHED_REPLACE_START();
  rc= memcached_send(ptr, key, key_length, value, value_length,
                         expiration, flags, "replace");
  LIBMEMCACHED_MEMCACHED_REPLACE_END();
  return rc;
}
