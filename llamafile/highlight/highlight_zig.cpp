// -*- mode:c++;indent-tabs-mode:nil;c-basic-offset:4;coding:utf-8 -*-
// vi: set et ft=cpp ts=4 sts=4 sw=4 fenc=utf-8 :vi
//
// Copyright 2024 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "highlight.h"
#include "util.h"
#include <cosmo.h>
#include <ctype.h>

enum
{
    NORMAL,
    WORD,
    QUOTE,
    QUOTE_BACKSLASH,
    DQUOTE,
    DQUOTE_BACKSLASH,
    SLASH,
    SLASH_SLASH,
    BACKSLASH,
    BACKSLASH_BACKSLASH,
};

HighlightZig::HighlightZig()
{
}

HighlightZig::~HighlightZig()
{
}

void
HighlightZig::feed(std::string* r, std::string_view input)
{
    for (size_t i = 0; i < input.size(); ++i) {
        wchar_t c;
        int b = input[i] & 255;
        if (!u_) {
            if (b < 0300) {
                c = b;
            } else {
                c_ = ThomPikeByte(b);
                u_ = ThomPikeLen(b) - 1;
                continue;
            }
        } else if (ThomPikeCont(b)) {
            c = c_ = ThomPikeMerge(c_, b);
            if (--u_)
                continue;
        } else {
            u_ = 0;
            c = b;
        }
        if (c == 0xFEFF)
            continue; // utf-8 bom
        switch (t_) {

        Normal:
        case NORMAL:
            if (!isascii(c) || isalpha(c) || c == '_' || c == '@') {
                t_ = WORD;
                lf::append_wchar(&word_, c);
            } else if (c == '/') {
                t_ = SLASH;
            } else if (c == '\\') {
                t_ = BACKSLASH;
            } else if (c == '\'') {
                t_ = QUOTE;
                *r += HI_STRING;
                *r += '\'';
            } else if (c == '"') {
                t_ = DQUOTE;
                *r += HI_STRING;
                *r += '"';
            } else {
                lf::append_wchar(r, c);
            }
            break;

        case WORD:
            if (!isascii(c) || isalnum(c) || c == '_') {
                lf::append_wchar(&word_, c);
            } else {
                if (is_keyword_zig(word_.data(), word_.size())) {
                    *r += HI_KEYWORD;
                    *r += word_;
                    *r += HI_RESET;
                } else if (is_keyword_zig_type(word_.data(), word_.size())) {
                    *r += HI_TYPE;
                    *r += word_;
                    *r += HI_RESET;
                } else if (is_keyword_zig_builtin(word_.data(), word_.size())) {
                    *r += HI_BUILTIN;
                    *r += word_;
                    *r += HI_RESET;
                } else if (is_keyword_zig_constant(word_.data(),
                                                   word_.size())) {
                    *r += HI_CONSTANT;
                    *r += word_;
                    *r += HI_RESET;
                } else {
                    *r += word_;
                }
                word_.clear();
                t_ = NORMAL;
                goto Normal;
            }
            break;

        case BACKSLASH:
            if (c == '\\') {
                *r += HI_STRING;
                *r += "\\\\";
                t_ = BACKSLASH_BACKSLASH;
            } else {
                *r += '\\';
                t_ = NORMAL;
                goto Normal;
            }
            break;

        case SLASH_SLASH:
        case BACKSLASH_BACKSLASH:
            lf::append_wchar(r, c);
            if (c == '\n') {
                *r += HI_RESET;
                t_ = NORMAL;
            }
            break;

        case SLASH:
            if (c == '/') {
                *r += HI_COMMENT;
                *r += "//";
                t_ = SLASH_SLASH;
            } else {
                *r += '/';
                t_ = NORMAL;
                goto Normal;
            }
            break;

        case QUOTE:
            lf::append_wchar(r, c);
            if (c == '\'') {
                *r += HI_RESET;
                t_ = NORMAL;
            } else if (c == '\\') {
                t_ = QUOTE_BACKSLASH;
            }
            break;

        case QUOTE_BACKSLASH:
            lf::append_wchar(r, c);
            t_ = QUOTE;
            break;

        case DQUOTE:
            lf::append_wchar(r, c);
            if (c == '"') {
                *r += HI_RESET;
                t_ = NORMAL;
            } else if (c == '\\') {
                t_ = DQUOTE_BACKSLASH;
            }
            break;

        case DQUOTE_BACKSLASH:
            lf::append_wchar(r, c);
            t_ = DQUOTE;
            break;

        default:
            __builtin_unreachable();
        }
    }
}

void
HighlightZig::flush(std::string* r)
{
    switch (t_) {
    case WORD:
        if (is_keyword_zig(word_.data(), word_.size())) {
            *r += HI_KEYWORD;
            *r += word_;
            *r += HI_RESET;
        } else if (is_keyword_zig_type(word_.data(), word_.size())) {
            *r += HI_TYPE;
            *r += word_;
            *r += HI_RESET;
        } else if (is_keyword_zig_builtin(word_.data(), word_.size())) {
            *r += HI_BUILTIN;
            *r += word_;
            *r += HI_RESET;
        } else if (is_keyword_zig_constant(word_.data(), word_.size())) {
            *r += HI_CONSTANT;
            *r += word_;
            *r += HI_RESET;
        } else {
            *r += word_;
        }
        word_.clear();
        break;
    case SLASH:
        *r += '/';
        break;
    case BACKSLASH:
        *r += '\\';
        break;
    case QUOTE:
    case QUOTE_BACKSLASH:
    case DQUOTE:
    case DQUOTE_BACKSLASH:
    case SLASH_SLASH:
    case BACKSLASH_BACKSLASH:
        *r += HI_RESET;
        break;
    default:
        break;
    }
    c_ = 0;
    u_ = 0;
    t_ = NORMAL;
}
