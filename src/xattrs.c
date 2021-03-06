/***
x509 attributes module for lua-openssl binding, Provide x509_attribute as lua object.
Sometime when you make CSR,TS or X509, you maybe need to use this.

@module x509.attr
@usage
  attr = require('openssl').x509.attr
*/
#include "openssl.h"
#include "private.h"
#include "sk.h"

#define MYNAME "x509.attribute"

/***
x509_attribute contrust param table.

@table x509_attribute_param_table
@tfield string|integer|asn1_object object, identify a asn1_object
@tfield string|integer type, same with type in asn1.new_string
@tfield string|asn1_object value, value of attribute

@usage
xattr = x509.attribute.new_attribute {
  object = asn1_object,
  type = Nid_or_String,
  value = string or asn1_string value
}
*/

/***
asn1_type object as table

@table asn1_type_table
@tfield string value, value data
@tfield string type, type of value
@tfield string format, value is 'der', only exist when type is not in 'bit','bmp','octet'
*/

/***
Create x509_attribute object

@function new_attribute
@tparam table attribute with object, type and value
@treturn[1] x509_attribute mapping to X509_ATTRIBUTE in openssl

@see x509_attribute_param_table
*/
static int openssl_xattr_new(lua_State*L)
{
  X509_ATTRIBUTE *x = NULL;
  luaL_checktable(L, 1);

  x = openssl_new_xattribute(L, &x, 1, NULL);
  PUSH_OBJECT(x, "openssl.x509_attribute");
  return 1;
}

static luaL_Reg R[] =
{
  {"new_attribute",         openssl_xattr_new},

  {NULL,          NULL},
};

/***
x509_attribute infomation table

@table x509_attribute_info_table
@tfield asn1_object|object object of asn1_object
@tfield boolean single  true for single value
@tfield table value  if single, value is asn1_type or array have asn1_type node table
*/
static int openssl_xattr_totable(lua_State*L, X509_ATTRIBUTE *attr)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  lua_newtable(L);
  openssl_push_asn1object(L, attr->object);
  lua_setfield(L, -2, "object");

  lua_newtable(L);
  if (attr->single)
  {
    openssl_push_asn1type(L, attr->value.single);
    lua_rawseti(L, -2, 1);
  }
  else
  {
    int i;
    for (i = 0; i < sk_ASN1_TYPE_num(attr->value.set); i++)
    {
      ASN1_TYPE* t = sk_ASN1_TYPE_value(attr->value.set, i);
      openssl_push_asn1type(L, t);
      lua_rawseti(L, -2, i + 1);
    }
  }
  lua_setfield(L, -2, "value");
  return 1;
#else
  int i, c;

  lua_newtable(L);
  openssl_push_asn1object(L, X509_ATTRIBUTE_get0_object(attr));
  lua_setfield(L, -2, "object");

  c = X509_ATTRIBUTE_count(attr);
  lua_newtable(L);
  for (i = 0; i < c; i++)
  {
    ASN1_TYPE* t = X509_ATTRIBUTE_get0_type(attr, i);
    openssl_push_asn1type(L, t);
    lua_rawseti(L, -2, i + 1);
  }
  lua_setfield(L, -2, "value");
  return 1;
#endif
}

/***
openssl.x509_attribute object
@type x509_attribute
*/

/***
get infomation table of x509_attribute.

@function info
@treturn[1] table info,  x509_attribute infomation as table
@see x509_attribute_info_table
*/
static int openssl_xattr_info(lua_State*L)
{
  X509_ATTRIBUTE* attr = CHECK_OBJECT(1, X509_ATTRIBUTE, "openssl.x509_attribute");
  return openssl_xattr_totable(L, attr);
}

/***
clone then asn1_attribute

@function dup
@treturn x509_attribute attr clone of x509_attribute
*/
static int openssl_xattr_dup(lua_State*L)
{
  X509_ATTRIBUTE* attr = CHECK_OBJECT(1, X509_ATTRIBUTE, "openssl.x509_attribute");
  X509_ATTRIBUTE* dup = X509_ATTRIBUTE_dup(attr);
  PUSH_OBJECT(dup, "openssl.x509_attribute");
  return 1;
}

static int openssl_xattr_free(lua_State*L)
{
  X509_ATTRIBUTE* attr = CHECK_OBJECT(1, X509_ATTRIBUTE, "openssl.x509_attribute");
  X509_ATTRIBUTE_free(attr);
  return 0;
}

/***
get type of x509_attribute

@function data
@tparam integer idx location want to get type
@tparam string attrtype attribute type
@treturn asn1_string
*/
/***
set type of x509_attribute

@function data
@tparam string attrtype attribute type
@tparam string data set to asn1_attr
@treturn boolean result true for success and others for fail
*/
static int openssl_xattr_data(lua_State*L)
{
  X509_ATTRIBUTE* attr = CHECK_OBJECT(1, X509_ATTRIBUTE, "openssl.x509_attribute");
  if (lua_type(L, 2) == LUA_TSTRING)
  {
    int attrtype = luaL_checkint(L, 2);
    size_t size;
    int ret;
    const char *data = luaL_checklstring(L, 3, &size);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    if (X509_ATTRIBUTE_count(attr) == 1)
      ASN1_TYPE_free((ASN1_TYPE*)attr->value.ptr);
    else
      sk_ASN1_TYPE_pop_free(attr->value.set, ASN1_TYPE_free);
    attr->value.ptr = NULL;
#else
#endif
    ret = X509_ATTRIBUTE_set1_data(attr, attrtype, data, size);
    return openssl_pushresult(L, ret);
  }
  else
  {
    int idx = luaL_checkint(L, 2);
    int attrtype = luaL_checkint(L, 3);
    ASN1_STRING *as = (ASN1_STRING *)X509_ATTRIBUTE_get0_data(attr, idx, attrtype, NULL);
    PUSH_ASN1_STRING(L, as);
    return 1;
  }
}

/***
get type of x509_attribute.

@function type
@tparam[opt] integer location which location to get type, default is 0
@treturn table asn1_type, asn1_type as table info
@treturn nil nil, fail return nothing
@see asn1_type_table
*/
static int openssl_xattr_type(lua_State*L)
{
  X509_ATTRIBUTE* attr = CHECK_OBJECT(1, X509_ATTRIBUTE, "openssl.x509_attribute");
  int loc = luaL_optint(L, 2, 0);
  ASN1_TYPE *type = X509_ATTRIBUTE_get0_type(attr, loc);
  if (type)
  {
    openssl_push_asn1type(L, type);;
    return 1;
  }
  else
    lua_pushnil(L);
  return 1;
}

/***
get asn1_object of x509_attribute.

@function object
@treturn asn1_object object of x509_attribute
*/
/***
set asn1_object for x509_attribute.

@function object
@tparam asn1_object obj
@treturn boolean true for success
@return nil when occure error, and followed by error message
*/
static int openssl_xattr_object(lua_State*L)
{
  X509_ATTRIBUTE* attr = CHECK_OBJECT(1, X509_ATTRIBUTE, "openssl.x509_attribute");
  if (lua_isnone(L, 2))
  {
    ASN1_OBJECT* obj = X509_ATTRIBUTE_get0_object(attr);
    openssl_push_asn1object(L, obj);
    return 1;
  }
  else
  {
    int nid = openssl_get_nid(L, 2);
    ASN1_OBJECT* obj;
    int ret;
    luaL_argcheck(L, nid != NID_undef, 2, "invalid asn1_object identity");
    obj = OBJ_nid2obj(nid);
    ret = X509_ATTRIBUTE_set1_object(attr, obj);
    return openssl_pushresult(L, ret);
  }
}

static luaL_Reg x509_attribute_funs[] =
{
  {"info",          openssl_xattr_info},
  {"dup",           openssl_xattr_dup},
  /* set or get */
  {"data",          openssl_xattr_data},
  {"type",          openssl_xattr_type},
  {"object",        openssl_xattr_object},

  {"__gc",          openssl_xattr_free},
  {"__tostring",    auxiliar_tostring},

  { NULL, NULL }
};

X509_ATTRIBUTE* openssl_new_xattribute(lua_State*L, X509_ATTRIBUTE** a, int idx, const char* eprefix)
{
  int arttype;
  size_t len = 0;
  int nid;
  const char* data = NULL;
  ASN1_STRING *s = NULL;

  lua_getfield(L, idx, "object");
  nid = openssl_get_nid(L, -1);
  if (nid == NID_undef)
  {
    if (eprefix)
    {
      luaL_error(L, "%s field object is invalid value", eprefix);
    }
    else
      luaL_argcheck(L, nid != NID_undef, idx, "field object is invalid value");
  }
  lua_pop(L, 1);

  lua_getfield(L, idx, "type");
  arttype = luaL_checkint(L, -1);
  if (arttype == V_ASN1_UNDEF || arttype == 0)
  {
    if (eprefix)
    {
      luaL_error(L, "%s field type is not invalid value", eprefix);
    }
    else
      luaL_argcheck(L, nid != NID_undef, idx, "field type is not invalid value");
  }
  lua_pop(L, 1);

  lua_getfield(L, idx, "value");
  if (lua_isstring(L, -1))
  {
    data = lua_tolstring(L, -1, &len);
  }
  else if ((s = GET_GROUP(-1, ASN1_STRING, "openssl.asn1group")) != NULL)
  {
    if (ASN1_STRING_type(s) != arttype)
    {
      if (eprefix)
        luaL_error(L, "%s field value not match type", eprefix);
      else
        luaL_argcheck(L, ASN1_STRING_type(s) == arttype, idx, "field value not match type");
    }
    data = (const char *)ASN1_STRING_get0_data(s);
    len  = ASN1_STRING_length(s);
  }
  else
  {
    if (eprefix)
    {
      luaL_error(L, "%s filed value only accept string or asn1_string", eprefix);
    }
    else
      luaL_argerror(L, idx, "filed value only accept string or asn1_string");
  }
  lua_pop(L, 1);
  if (data)
    return X509_ATTRIBUTE_create_by_NID(a, nid, arttype, data, len);
  return 0;
}


IMP_LUA_SK(X509_ATTRIBUTE, x509_attribute)

int openssl_register_xattribute(lua_State*L)
{
  auxiliar_newclass(L, "openssl.x509_attribute", x509_attribute_funs);
  lua_newtable(L);
  luaL_setfuncs(L, R, 0);
  return 1;
}
