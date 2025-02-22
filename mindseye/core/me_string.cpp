




size_t wcharToNarrow(const wchar_t * src, char * dest, size_t dest_len)
{
    size_t i;
    wchar_t code;
    i = 0;
    while (src[i] != '\0' && i < (dest_len - 1))
    {
        code = src[i];
        if (code < 128)
        {
            dest[i] = char(code);
        }
        else
        {
        dest[i] = '?';
        if (code >= 0xD800 && code <= 0xDBFF)
            // lead surrogate, skip the next code unit, which is the trail
            i++;
        }
        i++;
    }
    dest[i] = '\0';
    return i - 1;
}