//
// Copyright (c) 2007 Ole Andr� Vadla Ravn�s <oleavr@gmail.com>
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#include "stdafx.h"
#include <cctype>
#include "Signature.h"

namespace TrampoLib {

Signature::Signature(const SignatureSpec *spec)
{
    Initialize(spec);
}

void
Signature::Initialize(const SignatureSpec *spec)
{
    m_tokens.clear();
    m_length = 0;
    m_longestIndex = -1;
    m_longestOffset = -1;

    ParseSpec(spec);

    int longest = -1;
    int offset = 0;
    for (unsigned int i = 0; i < m_tokens.size(); i++)
    {
        SignatureToken &t = m_tokens[i];
        if (t.GetType() == TOKEN_TYPE_LITERAL)
        {
            int len = t.GetLength();
            if (len > longest)
            {
                longest = len;
                m_longestIndex = i;
                m_longestOffset = offset;
            }
        }

        offset += t.GetLength();
    }

    if (m_longestIndex < 0)
        throw runtime_error("no tokens found");
}

void
Signature::ParseSpec(const SignatureSpec *spec)
{
    OIStringStream iss(spec->signature, OIStringStream::in);

    while (iss.good())
    {
        if (isxdigit(iss.peek()))
        {
            SignatureToken token(TOKEN_TYPE_LITERAL);

            do
            {
                char buf[3];

                if (iss.get(buf, sizeof(buf)).good())
                {
                    OIStringStream bufIss(buf, OIStringStream::in);

                    unsigned int v;
                    bufIss >> hex >> v;

                    token += static_cast<char>(v);
                }

                while (isspace(iss.peek()) && iss.good())
                    iss.ignore(1);
            }
            while (isxdigit(iss.peek()) && iss.good());

            int length = token.GetLength();
            if (length == 0)
                throw runtime_error("invalid signature spec");

            m_length += token.GetLength();
            m_tokens.push_back(token);
        }
        else
        {
            int ignores = 0;

            do
            {
                char c = iss.peek();

                if (isspace(c) || c == '?')
                {
                    iss.ignore(1);
                    if (c == '?')  ignores++;
                }
                else
                {
                    break;
                }
            }
            while (iss.good());

            if (ignores > 0)
            {
                if (ignores % 2 != 0)
                    throw runtime_error("unbalanced questionmarks");

                SignatureToken token(TOKEN_TYPE_IGNORE, ignores / 2);

                m_length += token.GetLength();
                m_tokens.push_back(token);
            }
        }
    }
}

OVector<void *>::Type
SignatureMatcher::FindInRange(const Signature *sig, void *base, unsigned int size)
{
    OVector<void *>::Type matches;
    unsigned char *p = static_cast<unsigned char *>(base);
    unsigned char *maxP = p + size - sig->GetLength();

    for (; p <= maxP; p++)
    {
        const SignatureToken &t = sig->GetLongestToken();

        if (memcmp(p, t.GetData(), t.GetLength()) == 0)
        {
            void *candidateBase = p - sig->GetLongestTokenOffset();

            if (MatchesSignature(sig, candidateBase))
            {
                matches.push_back(candidateBase);

                // Skip ahead
                p = static_cast<unsigned char *>(candidateBase) + sig->GetLength();
            }
        }
    }

    return matches;
}

bool
SignatureMatcher::MatchesSignature(const Signature *sig, void *base)
{
    unsigned char *p = static_cast<unsigned char *>(base);

    for (unsigned int i = 0; i < sig->GetTokenCount(); i++)
    {
        const SignatureToken &t = (*sig)[i];

        if (t.GetType() == TOKEN_TYPE_LITERAL)
        {
            if (memcmp(p, t.GetData(), t.GetLength()) != 0)
            {
                return false;
            }
        }
    }

    return true;
}

} // namespace TrampoLib