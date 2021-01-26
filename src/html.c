/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include "common/utils.h"
#include "html.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static char *html_skip_spaces(char *string, size_t size)
{
	char *res = string;

	while (size > 0 && (*res == '\r' || *res == '\n' || *res == ' ' || *res == '\t')) {
		res++;
		size--;
	}

	return res;
}

static char *html_find_next_tag(char *string, size_t size, char **tag, char **property, bool * istag, bool stripeol)
{
	char *res = string;
	bool inquote = false;
	char quote = ' ';
	char *old;

	*property = NULL;

	while (size > 0 && *res != '\0' && *res != '<' && *res != '&') {
		if (stripeol && (*res == ' ' || *res == '\t' || *res == '\r' || *res == '\n')) {
			if (res > string && *(res - 1) == ' ')
				*res = 0x1F;
			else
				*res = ' ';
		}

		res++;
		size--;
	}

	if (size == 0)
		return res;

	if (*res == 0)
		return NULL;

	if (*res == '<') {
		*res++ = 0;
		size--;

		if (size == 0)
			return res;

		old = res;
		res = html_skip_spaces(res, size);
		size -= res - old;

		if (size == 0)
			return res;

		*tag = res;

		while (size > 0 && (inquote || (*res != '\0' && *res != '>'))) {
			if (*res == '\"' || *res == '\'') {
				if (!inquote) {
					inquote = true;
					quote = *res;
				} else if (quote == *res)
					inquote = false;

				res++;
				size--;

				if (size == 0)
					return res;

			} else if (!inquote && (*res == ' ' || *res == '\t' || *res == '\r' || *res == '\n')) {
				*res = 0;
				res++;
				size--;

				if (size == 0)
					return res;

				old = res;
				res = html_skip_spaces(res, size);
				size -= res - old;

				if (size == 0)
					return res;

				if (*res != '>')
					*property = res;
			} else {
				res++;
				size--;

				if (size == 0)
					return res;
			}
		}

		if (*res == 0)
			return NULL;

		*res++ = 0;
		size--;

		if (size == 0)
			return res;

		old = res;
		res = html_skip_spaces(res, size);
		size -= res - old;

		if (size == 0)
			return res;

		*istag = true;
	} else {
		*res++ = 0;
		size--;

		if (size == 0)
			return res;

		*tag = res;

		while (size > 0 && *res != '\0' && *res != ' ' && *res != ';' && *res != '<' && *res != '&') {
			res++;
			size--;
		}

		if (size == 0)
			return res;

		if (*res == 0)
			return NULL;

		if (*res == ' ' || *res == ';') {
			*res++ = 0;
			size--;

			if (size == 0)
				return res;
		} else
			*(res - 1) = 0;

		*istag = false;
	}

	return res;
}

static char *html_skip_close_tag(char *string, size_t size, char *tag)
{
	char *res = string, *sstart;
	int len = strnlen(tag, size) + 1;
	char ttag[len + 1];

	snprintf_s(ttag, len + 1, "/%s", tag);

	do {
		while (size > 0 && *res != 0 && *res != '<') {
			res++;
			size--;
		}

		if (size == 0)
			return res;

		if (*res == 0)
			return NULL;

		sstart = res;
		res++;
		size--;

		if (size == 0)
			return res;

		res = html_skip_spaces(res, size);

		if (strncmp(ttag, res, len) == 0)
			res += len;
		else
			continue;

		res = html_skip_spaces(res, size);
		if (*res == 0)
			return NULL;
	} while (*res != '>');

	*sstart = *res++ = 0;

	size--;

	if (size == 0)
		return res;

	return res;
}

extern u32 html_to_text(char *string, u32 size, bool stripeol)
{
	char *nstr = NULL, *ntag = string, *cstr = string, *str = string, *prop = NULL;
	bool istag = false;

	if (size > 0) {
		char *cur, *end;

		for (cur = string, end = cur + size; cur < end; cur++)
			if (*cur == 0)
				*cur = 1;
	}

	while (str != NULL && (nstr = html_find_next_tag(str, size, &ntag, &prop, &istag, stripeol)) != NULL) {
		int len = (int) ntag - (int) str - 1;

		if (len > 0) {
			if (str != cstr) {
				memmove(cstr, str, len);
			}
			cstr += len;
			size -= len;

			if (size == 0) {
				return cstr - string;
			}
		} else if (len == -1) {
			if (str != cstr) {
				memmove(cstr, str, len);
			}

			cstr += size;
			size = 0;

			return cstr - string;
		}

		if (istag) {
			if (stricmp(ntag, "head") == 0 || stricmp(ntag, "style") == 0 || stricmp(ntag, "title") == 0) {
				char *pstr = html_skip_close_tag(nstr, size, ntag);

				if (pstr != NULL)
					str = pstr;
				else
					str = nstr;
			} else if (stricmp(ntag, "script") == 0) {
				char *kstr = nstr;

				str = html_skip_close_tag(nstr, size, ntag);
				while ((kstr = strstr(kstr, "document.write(")) != NULL) {
					kstr += 15;
					while (*kstr == ' ' || *kstr == '\t')
						kstr++;
					if (*kstr == '\'' || *kstr == '"') {
						char qt = *kstr;
						char *dstr;

						kstr++;
						dstr = kstr;
						while (*dstr != 0 && (*dstr != qt || *(dstr - 1) == '\\'))
							dstr++;
						if (*dstr == qt) {
							strncpy(cstr, kstr, dstr - kstr);
							cstr[dstr - kstr] = '\0';
							cstr += html_to_text(cstr, dstr - kstr, true);
							size -= dstr - kstr;

							if (size == 0) {
								return cstr - string;
							}
						}
						kstr = dstr;
					}
				}
			} else if (stricmp(ntag, "pre") == 0) {
				char *pstr = html_skip_close_tag(nstr, size, "pre");

				if (pstr != NULL) {
					len = pstr - nstr - 6;
					strncpy(cstr, nstr, len);
					cstr[len] = '\0';
					cstr += html_to_text(cstr, len, false);
					size -= len;

					if (size == 0) {
						return cstr - string;
					}
				}
				str = pstr;
			} else {
				if (stricmp(ntag, "br") == 0
					|| stricmp(ntag, "li") == 0
					|| stricmp(ntag, "p") == 0
					|| stricmp(ntag, "/p") == 0
					|| stricmp(ntag, "tr") == 0 || stricmp(ntag, "/table") == 0 || stricmp(ntag, "div") == 0 || stricmp(ntag, "/div") == 0) {
					if (prop != NULL) {
						char *v = strchr(prop, '=');

						if (v == NULL) {
							*cstr++ = '\n';
							size--;

							if (size == 0) {
								return cstr - string;
							}
						} else {
							char *t = v - 1;

							*v = 0;
							while (t > prop && (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n'))
								t--;
							*(t + 1) = 0;
							if (stricmp(prop, "style") == 0) {
								v++;
								v = html_skip_spaces(v, size);
								if (*v == '"' || *v == '\'') {
									char *rq = strchr(v + 1,
													  *v);

									if (rq == NULL) {
										*cstr++ = '\n';
										size--;

										if (size == 0) {
											return cstr - string;
										}
										v = NULL;
									} else {
										*rq = 0;
										v++;
									}
								}
								if (v != NULL) {
									if (stricmp(v, "display:none") != 0) {
										*cstr++ = '\n';
										size--;

										if (size == 0) {
											return cstr - string;
										}
									} else {
										char *pstr = html_skip_close_tag(nstr, size,
																		 ntag);

										if (pstr != NULL)
											nstr = pstr;
									}
								}
							}
						}
					} else {
						*cstr++ = '\n';
						size--;

						if (size == 0) {
							return cstr - string;
						}
					}
				} else if (stricmp(ntag, "th") == 0 || stricmp(ntag, "td") == 0) {
					*cstr++ = ' ';
					size--;

					if (size == 0) {
						return cstr - string;
					}
				}
				str = nstr;
			}
		} else {
			if (stricmp(ntag, "nbsp") == 0) {
				*cstr++ = ' ';
				size--;

				if (size == 0) {
					return cstr - string;
				}

			} else if (stricmp(ntag, "gt") == 0) {
				*cstr++ = '>';
				size--;

				if (size == 0) {
					return cstr - string;
				}

			} else if (stricmp(ntag, "lt") == 0) {
				*cstr++ = '<';
				size--;

				if (size == 0) {
					return cstr - string;
				}

			} else if (stricmp(ntag, "amp") == 0) {
				*cstr++ = '&';
				size--;

				if (size == 0) {
					return cstr - string;
				}

			} else if (stricmp(ntag, "quote") == 0) {
				*cstr++ = '\'';
				size--;

				if (size == 0) {
					return cstr - string;
				}

			} else if (stricmp(ntag, "copy") == 0) {
				*cstr++ = '(';
				size--;

				if (size == 0) {
					return cstr - string;
				}

				*cstr++ = 'C';
				size--;

				if (size == 0) {
					return cstr - string;
				}

				*cstr++ = ')';
				size--;

				if (size == 0) {
					return cstr - string;
				}

			}
			str = nstr;
		}
	}

	if (str != NULL) {
		int slen = strnlen(str, size);

		memmove(cstr, str, slen);
		cstr += slen;
	}

	return cstr - string;
}
