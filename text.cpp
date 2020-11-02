#include "text.h"
#include "iconv.h"
#include "constants.h"
#include <signal.h>
#include <iostream>


text::text()
{

}

text::text(const char* byte_string, const std::string& encoding)
{
  assign(byte_string, encoding);
}

text::text(const wchar_t* wide_string)
{
  assign(wide_string);
}

void text::assign(const char* byte_string, const std::string& encoding)
{
  init_byte_encoding = encoding;
  init_byte_string.assign(byte_string);

  if (encoding == "UTF-8")
    cached_byte_string.assign(byte_string);
}

void text::assign(const wchar_t* wide_string)
{
  cached_wide_string.assign(wide_string);
}

const std::wstring& text::wide_string() const
{
  if (cached_wide_string.empty())
    if (!init_byte_string.empty())
      _iconv(init_byte_string, init_byte_encoding.c_str(), cached_wide_string, WCHAR_T_PLATFORM_ENCODING);

  return cached_wide_string;
}

const std::string& text::byte_string() const
{
  if (cached_byte_string.empty())
  {
    if (!cached_wide_string.empty())
      _iconv(cached_wide_string.c_str(), WCHAR_T_PLATFORM_ENCODING, cached_byte_string, "UTF-8");
    else if (!init_byte_string.empty())
      _iconv(init_byte_string, init_byte_encoding.c_str(), cached_byte_string, "UTF-8");
  }

  return cached_byte_string;
}

const std::string& text::byte_string(const std::string& encoding) const
{
  auto found = cached_byte_strings.find(encoding);
  if (found != cached_byte_strings.end())
    return found->second;
  else
    if (!init_byte_string.empty())
    {
      std::string new_string;
      _iconv(init_byte_string, init_byte_encoding.c_str(), new_string, encoding.c_str());

      return cached_byte_strings.insert(std::pair<std::string, std::string>(encoding, std::move(new_string))).first->second;
    }

  static std::string ret;
  return ret;
}

const std::u32string& text::unicode_string() const
{
  if (cached_unicode_string.empty())
  {
    switch (sizeof(wchar_t))
    {
    case 1://TODO: optimization select an existing string as source for converting
      convert_utf16_to_utf32(wide_string(), cached_unicode_string);
      break;
    case 2://TODO: optimization select an existing string as source for converting
      convert_utf16_to_utf32(wide_string(), cached_unicode_string);
      break;
    case 4:
      cached_unicode_string.assign((char32_t*)wide_string().c_str());
      break;
    default:
      break;
    }
  }

  return cached_unicode_string;
}

const char32_t& text::operator[](int index) const
{
  return unicode_string()[index];
}

char32_t& text::operator[](int index)
{
  return const_cast<char32_t&>(unicode_string()[index]);
}

text* text::text::operator->()
{
  cached_byte_string.clear();
  cached_wide_string.clear();
  cached_unicode_string.clear();

  init_byte_encoding.clear();
  init_byte_string.clear();
  
  cached_byte_strings.clear();

  return this;
}

size_t text::_iconv(const char* instr, const char* in_encode,
  std::string& outstr, const char* out_encode) const
{
  char* res = nullptr;
  size_t insize = strlen(instr) + 1;
  size_t outsize = insize * UTF8_SEQUENCE_MAXLEN;
  const auto ret = _iconv_internal(instr, in_encode, insize, &res, out_encode, outsize);
  outstr.assign(res);
  delete[] res;
  return ret;
}

size_t text::_iconv(const char* instr, const char* in_encode,
  std::wstring& outstr, const char* out_encode) const
{
  char* res = nullptr;
  size_t insize = strlen(instr) + 1;
  size_t outsize = insize * UTF8_SEQUENCE_MAXLEN;
  const auto ret =_iconv_internal(instr, in_encode, insize, &res, out_encode, outsize);
  outstr.assign((wchar_t*)res);
  delete[] res;
  return ret;
}

size_t text::_iconv(const wchar_t* instr, const char* in_encode,
  std::string& outstr, const char* out_encode) const
{
  char* res = nullptr;
  const size_t wlen = (wcslen(instr) + 1);
  size_t insize = wlen * sizeof(wchar_t);
  size_t outsize = wlen * UTF8_SEQUENCE_MAXLEN;
  const auto ret = _iconv_internal((const char*)instr, in_encode, insize, &res, out_encode, outsize);
  outstr.assign(res);
  delete[] res;
  return ret;
}

size_t text::_iconv(const wchar_t* instr, const char* in_encode,
  std::wstring& outstr, const char* out_encode) const
{
  char* res = nullptr;
  const size_t wlen = (wcslen(instr) + 1);
  size_t insize = wlen * sizeof(wchar_t);
  size_t outsize = wlen * UTF8_SEQUENCE_MAXLEN;
  const auto ret = _iconv_internal((const char*)instr, in_encode, insize, &res, out_encode, outsize);
  outstr.assign((wchar_t*)res);
  delete[] res;
  return ret;
}

size_t text::_iconv_internal(const char* instr, const char* in_encode, size_t& insize,
  char** outstr, const char* out_encode, size_t& outsize) const
{
  auto result = new char[outsize];

  const size_t outsize_saved = outsize;

#ifdef __linux__
  auto inptr = (char*)instr;
#elif _WIN32
  auto inptr = (const char*)instr;
#else
#endif

  auto cd = iconv_open(out_encode, in_encode);
  if (cd == (iconv_t)-1)
  {
    if (errno == EINVAL)
      std::cerr << "conversion from " << in_encode << " to " << out_encode << " not available\n";
    else
      std::cerr << "iconv_open\n";
  }

  auto outptr = result;
  auto nconv = iconv(cd, &inptr, &insize, &outptr, &outsize);
  if (nconv == (size_t)-1)
  {
    if (errno == EINVAL)
      std::cerr << "This is harmless.\n";
    else
      std::cerr << "It is a real problem!\n";
  }

  iconv_close(cd);

  *outstr = result;

  return outsize_saved - outsize;
}

void text::convert_utf16_to_utf32(const wchar_t* instr, std::u32string& outstr) const
{
  size_t size = wcslen(instr) + 1 + 1;
  auto output = new char32_t[size];
  const char32_t* const result = output;
  const wchar_t* const end = instr + size;
  while (instr < end) {
    const wchar_t uc = *instr++;
    if (!is_surrogate(uc)) {
      *output++ = uc;
    }
    else {
      if (is_high_surrogate(uc) && instr < end && is_low_surrogate(*instr))
        *output++ = surrogate_to_utf32(uc, *instr++);
      else
        std::cerr << "ERROR!\n";
    }
  }
  *(++output) = U'\0';

  cached_unicode_string.assign(result);

  delete[] output;
}
