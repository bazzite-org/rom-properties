/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * RpFile_IStream.hpp: IRpFile using an IStream*.                          *
 *                                                                         *
 * Copyright (c) 2016-2019 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "stdafx.h"
#include "RpFile_IStream.hpp"

// librpbase
#include "librpbase/byteswap.h"
#include "librpbase/TextFuncs.hpp"
#include "librpbase/file/IRpFile.hpp"
using namespace LibRpBase;

// C++ includes.
#include <memory>
#include <string>
using std::string;
using std::unique_ptr;

// zlib for transparent gzip decompression.
#include <zlib.h>

// zlib buffer size.
#define ZLIB_BUFFER_SIZE 16384

#ifdef _MSC_VER
// MSVC: Exception handling for /DELAYLOAD.
#include "libwin32common/DelayLoadHelper.h"
#endif /* _MSC_VER */

#ifdef _MSC_VER
// DelayLoad test implementation.
DELAYLOAD_TEST_FUNCTION_IMPL0(zlibVersion);
#endif /* _MSC_VER */

/**
 * Create an IRpFile using IStream* as the underlying storage mechanism.
 * @param pStream	[in] IStream*.
 * @param gzip		[in] If true, handle gzipped files automatically.
 */
RpFile_IStream::RpFile_IStream(IStream *pStream, bool gzip)
	: super()
	, m_pStream(pStream)
	, m_z_uncomp_sz(0)
	, m_z_filepos(0)
	, m_z_realpos(0)
	, m_pZstm(nullptr)
	// zlib buffer
	, m_pZbuf(nullptr)
	, m_zbufLen(0)
	, m_zcurPos(0)
{
	pStream->AddRef();

	if (gzip) {
#if defined(_MSC_VER) && defined(ZLIB_IS_DLL)
		// Delay load verification.
		// TODO: Only if linked with /DELAYLOAD?
		if (DelayLoad_test_zlibVersion() != 0) {
			// Delay load failed.
			// Don't do any gzip checking.
			return;
		}
#endif /* defined(_MSC_VER) && defined(ZLIB_IS_DLL) */

		// for IStream::Seek()
		LARGE_INTEGER li;

		// Check for a gzipped file.
		uint16_t gzmagic;
		ULONG cbRead;
		HRESULT hr = m_pStream->Read(&gzmagic, (ULONG)sizeof(gzmagic), &cbRead);
		if (SUCCEEDED(hr) && cbRead == (ULONG)sizeof(gzmagic) &&
		    gzmagic == be16_to_cpu(0x1F8B))
		{
			// gzip magic found!
			// Get the uncompressed size at the end of the file.
			ULARGE_INTEGER uliFileSize;
			li.QuadPart = -4;
			hr = m_pStream->Seek(li, STREAM_SEEK_END, &uliFileSize);
			if (SUCCEEDED(hr)) {
				uliFileSize.QuadPart += 4;
				hr = m_pStream->Read(&m_z_uncomp_sz, (ULONG)sizeof(m_z_uncomp_sz), &cbRead);
				if (SUCCEEDED(hr) && cbRead == (ULONG)sizeof(m_z_uncomp_sz)) {
					m_z_uncomp_sz = le32_to_cpu(m_z_uncomp_sz);
					if (m_z_uncomp_sz >= uliFileSize.QuadPart-(10+8)) {
						// Valid filesize.
						// Initialize zlib.
						// NOTE: m_pZstm *must* be zero-initialized.
						// Otherwise, inflateInit() will crash.
						m_pZstm = static_cast<z_stream*>(calloc(1, sizeof(z_stream)));
						if (m_pZstm) {
							int err = inflateInit2(m_pZstm, 16+MAX_WBITS);
							if (err != Z_OK) {
								// Error initializing zlib.
								free(m_pZstm);
								m_pZstm = nullptr;
								m_z_uncomp_sz = 0;
							}

							// Allocate the zlib buffer.
							m_pZbuf = static_cast<uint8_t*>(malloc(ZLIB_BUFFER_SIZE));
							if (!m_pZbuf) {
								// malloc() failed.
								inflateEnd(m_pZstm);
								free(m_pZstm);
								m_pZstm = nullptr;
							}
						}
					}
				}
			}
		}

		if (!m_pZstm) {
			// Error initializing zlib.
			m_z_uncomp_sz = 0;
		}

		// Rewind back to the beginning of the stream.
		li.QuadPart = 0;
		m_pStream->Seek(li, STREAM_SEEK_SET, nullptr);
	}
}

RpFile_IStream::~RpFile_IStream()
{
	free(m_pZbuf);

	if (m_pZstm) {
		// Close zlib.
		inflateEnd(m_pZstm);
		free(m_pZstm);
	}

	if (m_pStream) {
		m_pStream->Release();
	}
}

/**
 * Copy the zlib stream from another RpFile_IStream.
 * @param other
 * @return 0 on success; non-zero on error.
 */
int RpFile_IStream::copyZlibStream(const RpFile_IStream &other)
{
	int ret = 0;

	// Delete the current stream.
	if (m_pZstm) {
		inflateEnd(m_pZstm);
		free(m_pZstm);
	}
	free(m_pZbuf);

	if (!other.m_pZstm) {
		// No stream to copy.
		// Zero everything out.
		goto zero_all_values;
	}

	// Copy the stream.
	m_pZstm = static_cast<z_stream*>(malloc(sizeof(z_stream)));
	if (!m_pZstm) {
		// Error allocating the z_stream.
		m_lastError = ENOMEM;
		ret = -1;
		goto zero_all_values;
	}
	m_pZbuf = static_cast<uint8_t*>(malloc(ZLIB_BUFFER_SIZE));
	if (!m_pZbuf) {
		// Error allocating the zlib buffer.
		free(m_pZstm);
		m_lastError = ENOMEM;
		ret = -1;
		goto zero_all_values;
	}		

	// Copy the zlib stream.
	// NOTE: inflateCopy() handles the internal_state struct.
	inflateCopy(m_pZstm, other.m_pZstm);
	m_pZstm->next_in = nullptr;
	m_pZstm->next_out = nullptr;

	// Copy the zlib buffer.
	memcpy(m_pZbuf, other.m_pZbuf, other.m_zbufLen);

	// Copy the other values.
	m_z_uncomp_sz = other.m_z_uncomp_sz;
	m_z_filepos = other.m_z_filepos;
	m_z_realpos = other.m_z_realpos;
	m_zbufLen = other.m_zbufLen;
	m_zcurPos = other.m_zcurPos;

	// We're done here.
	return 0;

zero_all_values:
	// Zero everything.
	m_pZstm = nullptr;
	m_pZbuf = nullptr;

	m_z_uncomp_sz = 0;
	m_z_filepos = 0;
	m_z_realpos = 0;
	m_zbufLen = 0;
	m_zcurPos = 0;

	return ret;
}

/**
 * Copy constructor.
 * @param other Other instance.
 */
RpFile_IStream::RpFile_IStream(const RpFile_IStream &other)
	: super()
	, m_pStream(other.m_pStream)
	, m_z_uncomp_sz(0)
	, m_z_filepos(0)
	, m_z_realpos(0)
	, m_pZstm(nullptr)
	// zlib buffer
	, m_pZbuf(nullptr)
	, m_zbufLen(0)
	, m_zcurPos(0)
{
	// TODO: Combine with the assignment constructor?

	int ret = copyZlibStream(other);
	if (ret != 0) {
		// Error copying the zlib stream.
		m_pStream = nullptr;
		return;
	}

	// Take a reference to the other IStream.
	m_pStream->AddRef();
	m_lastError = other.m_lastError;

	// Nothing else to do, since we can't actually
	// clone the stream.
}

/**
 * Assignment operator.
 * @param other Other instance.
 * @return This instance.
 */
RpFile_IStream &RpFile_IStream::operator=(const RpFile_IStream &other)
{
	// TODO: Combine with copy constructor?

	// If we have a stream open, close it first.
	if (m_pStream) {
		m_pStream->Release();
		m_pStream = nullptr;
	}

	// Copy the zlib stream.
	int ret = copyZlibStream(other);
	if (ret != 0) {
		// Error copying the zlib stream.
		return *this;
	}

	// Take a reference to the other IStream.
	m_pStream = other.m_pStream;
	m_pStream->AddRef();

	// Nothing else to do, since we can't actually
	// clone the stream.
	m_lastError = other.m_lastError;
	return *this;
}

/**
 * Is the file open?
 * This usually only returns false if an error occurred.
 * @return True if the file is open; false if it isn't.
 */
bool RpFile_IStream::isOpen(void) const
{
	return (m_pStream != nullptr);
}

/**
 * dup() the file handle.
 *
 * Needed because IRpFile* objects are typically
 * pointers, not actual instances of the object.
 *
 * NOTE: The dup()'d IRpFile* does NOT have a separate
 * file pointer. This is due to how dup() works.
 *
 * @return dup()'d file, or nullptr on error.
 */
IRpFile *RpFile_IStream::dup(void)
{
	return new RpFile_IStream(*this);
}

/**
 * Close the file.
 */
void RpFile_IStream::close(void)
{
	if (m_pStream) {
		m_pStream->Release();
		m_pStream = nullptr;
	}
}

/**
 * Read data from the file.
 * @param ptr Output data buffer.
 * @param size Amount of data to read, in bytes.
 * @return Number of bytes read.
 */
size_t RpFile_IStream::read(void *ptr, size_t size)
{
	HRESULT hr;

	if (!m_pStream) {
		m_lastError = EBADF;
		return 0;
	}

	if (m_pZstm) {
		// Read and decompress.
		// Reference: https://www.codeproject.com/Articles/3602/Zlib-compression-decompression-wrapper-as-ISequent
		m_pZstm->next_out = static_cast<Bytef*>(ptr);
		m_pZstm->avail_out = static_cast<uInt>(size);

		// Only seek if we need to read data.
		bool didSeek = false;

		if (m_zcurPos == m_zbufLen) {
			// Need to read more data from the gzipped file.
			if (!didSeek) {
				// Seek to the last real position.
				LARGE_INTEGER dlibMove;
				dlibMove.QuadPart = m_z_realpos;
				hr = m_pStream->Seek(dlibMove, STREAM_SEEK_SET, nullptr);
				if (FAILED(hr)) {
					// Unable to seek.
					m_lastError = EIO;
					return 0;
				}
				didSeek = true;
			}
			// S_FALSE: End of file. Continue processing with
			//          whatever's left.
			// E_PENDING: TODO
			// Other: Error.
			hr = m_pStream->Read(m_pZbuf, ZLIB_BUFFER_SIZE, &m_zbufLen);
			if (FAILED(hr) && hr != S_FALSE) {
				// Read error.
				m_lastError = EIO;
				return 0;
			}
			m_z_realpos += m_zbufLen;
			m_zcurPos = 0;
		}

		int err = Z_OK;
		do {
			m_pZstm->next_in = &m_pZbuf[m_zcurPos];
			m_pZstm->avail_in = m_zbufLen - m_zcurPos;

			if (m_pZstm->avail_in == 0) {
				// Out of data.
				return 0;
			}

			err = inflate(m_pZstm, Z_SYNC_FLUSH);
			switch (err) {
				case Z_OK:
				case Z_STREAM_END:
					// Data decompressed successfully.
					// NOTE: Z_STREAM_END occurs if we reached
					// the end of the stream in this read.
					break;
				default:
					// Error decompressing data.
					m_lastError = EIO;
					return 0;
			}

			if (m_pZstm->avail_out == 0) {
				m_zcurPos = static_cast<unsigned int>(m_pZstm->next_in - m_pZbuf);
				break;
			}

			// Read more data from the gzipped file.
			if (!didSeek) {
				// Seek to the last real position.
				LARGE_INTEGER dlibMove;
				dlibMove.QuadPart = m_z_realpos;
				hr = m_pStream->Seek(dlibMove, STREAM_SEEK_SET, nullptr);
				if (FAILED(hr)) {
					// Unable to seek.
					m_lastError = EIO;
					return 0;
				}
				didSeek = true;
			}

			m_zcurPos = 0;
			hr = m_pStream->Read(m_pZbuf, ZLIB_BUFFER_SIZE, &m_zbufLen);
			// S_FALSE: End of file. Continue processing with
			//          whatever's left.
			// E_PENDING: TODO
			// Other: Error.
			if (FAILED(hr) && hr != S_FALSE) {
				// Read error.
				m_lastError = EIO;
				m_zbufLen = 0;
				return 0;
			}
			m_z_realpos += m_zbufLen;
		} while (m_pZstm->avail_out > 0);

		// Adjust the current seek pointer based on how much data was read.
		const unsigned int sz_read = static_cast<unsigned int>(size - m_pZstm->avail_out);
		m_z_filepos += sz_read;

		// We're done here.
		return sz_read;
	}

	ULONG cbRead;
	hr = m_pStream->Read(ptr, (ULONG)size, &cbRead);
	if (FAILED(hr)) {
		// An error occurred.
		// TODO: Convert hr to POSIX?
		m_lastError = EIO;
		return 0;
	}

	return (size_t)cbRead;
}

/**
 * Write data to the file.
 * @param ptr Input data buffer.
 * @param size Amount of data to read, in bytes.
 * @return Number of bytes written.
 */
size_t RpFile_IStream::write(const void *ptr, size_t size)
{
	if (!m_pStream) {
		// TODO: Read-only check?
		m_lastError = EBADF;
		return 0;
	}

	// Cannot write to zlib streams.
	if (m_pZstm) {
		m_lastError = EROFS;
		return 0;
	}

	ULONG cbWritten;
	HRESULT hr = m_pStream->Write(ptr, (ULONG)size, &cbWritten);
	if (FAILED(hr)) {
		// An error occurred.
		// TODO: Convert HRESULT to POSIX?
		m_lastError = EIO;
		return 0;
	}

	return (size_t)cbWritten;
}

/**
 * Set the file position.
 * @param pos File position.
 * @return 0 on success; -1 on error.
 */
int RpFile_IStream::seek(int64_t pos)
{
	LARGE_INTEGER dlibMove;
	HRESULT hr;

	if (!m_pStream) {
		m_lastError = EBADF;
		return -1;
	}

	if (m_pZstm) {
		// zlib stream: Special seek handling.
		if (pos == m_z_filepos) {
			// No seek necessary.
			return 0;
		}

		size_t skip_bytes;
		if (pos < m_z_filepos) {
			// Reset the stream first.
			inflateEnd(m_pZstm);

			m_z_filepos = 0;
			m_zbufLen = 0;
			m_zcurPos = 0;
			memset(m_pZstm, 0, sizeof(*m_pZstm));
			int err = inflateInit2(m_pZstm, 16+MAX_WBITS);

			// Seek to the beginning of the real file.
			m_z_realpos = 0;
			dlibMove.QuadPart = 0;
			hr = m_pStream->Seek(dlibMove, STREAM_SEEK_SET, nullptr);

			if (err != Z_OK || FAILED(hr)) {
				// Error initializing the zlib stream
				// and/or rewinding the base stream.
				// Cannot continue with this stream.
				free(m_pZstm);
				free(m_pZbuf);
				m_pZstm = nullptr;
				m_pZbuf = nullptr;
				m_z_uncomp_sz = 0;

				m_pStream->Release();
				m_pStream = nullptr;
				return -1;
			}

			skip_bytes = static_cast<size_t>(pos);
		} else if (pos > m_z_filepos) {
			// Skip over the bytes.
			skip_bytes = static_cast<size_t>(pos - m_z_filepos);
		} else {
			// Should not happen...
			assert(!"Something happened...");
			return -1;
		}

		// Skip over the required number of bytes.
		unique_ptr<uint8_t[]> skip_buf(new uint8_t[ZLIB_BUFFER_SIZE]);
		while (skip_bytes > 0) {
			size_t sz_to_read = (skip_bytes > ZLIB_BUFFER_SIZE ? ZLIB_BUFFER_SIZE : skip_bytes);
			size_t sz_read = this->read(skip_buf.get(), sz_to_read);
			if (sz_read != sz_to_read) {
				// Error...
				m_lastError = -EIO;
				return -1;
			}

			skip_bytes -= sz_to_read;
		}

		// Seek was successful.
		return 0;
	}

	// Seek in the base stream.
	dlibMove.QuadPart = pos;
	hr = m_pStream->Seek(dlibMove, STREAM_SEEK_SET, nullptr);
	if (FAILED(hr)) {
		// TODO: Convert hr to POSIX?
		m_lastError = EIO;
		return -1;
	}

	if (m_pZstm) {
		// Reset the zlib stream.
		m_z_filepos = 0;
		m_z_realpos = 0;
		m_zbufLen = 0;
		m_zcurPos = 0;
		inflateEnd(m_pZstm);
		memset(m_pZstm, 0, sizeof(*m_pZstm));
		int err = inflateInit2(m_pZstm, 16+MAX_WBITS);
		if (err != Z_OK) {
			// Error initializing the zlib stream...
			// Cannot continue with this stream.
			free(m_pZstm);
			free(m_pZbuf);
			m_pZstm = nullptr;
			m_pZbuf = nullptr;
			m_z_uncomp_sz = 0;

			m_pStream->Release();
			m_pStream = nullptr;
			return -1;
		}
	}

	return 0;
}

/**
 * Get the file position.
 * @return File position, or -1 on error.
 */
int64_t RpFile_IStream::tell(void)
{
	if (!m_pStream) {
		m_lastError = EBADF;
		return -1;
	}

	if (m_pZstm) {
		// zlib-compressed file.
		return static_cast<int64_t>(m_z_filepos);
	}

	LARGE_INTEGER dlibMove;
	ULARGE_INTEGER ulibNewPosition;
	dlibMove.QuadPart = 0;
	HRESULT hr = m_pStream->Seek(dlibMove, STREAM_SEEK_CUR, &ulibNewPosition);
	if (FAILED(hr)) {
		// TODO: Convert HRESULT to POSIX?
		m_lastError = EIO;
		return -1;
	}

	return (int64_t)ulibNewPosition.QuadPart;
}

/**
 * Truncate the file.
 * @param size New size. (default is 0)
 * @return 0 on success; -1 on error.
 */
int RpFile_IStream::truncate(int64_t size)
{
	// TODO: Needs testing.
	if (!m_pStream) {
		m_lastError = EBADF;
		return -1;
	} else if (size < 0) {
		m_lastError = EINVAL;
		return -1;
	} else if (m_pZstm) {
		// zlib is read-only.
		m_lastError = EROFS;
		return -1;
	}

	// Get the current stream position.
	LARGE_INTEGER dlibMove;
	ULARGE_INTEGER ulibNewPosition;
	dlibMove.QuadPart = 0;
	HRESULT hr = m_pStream->Seek(dlibMove, STREAM_SEEK_CUR, &ulibNewPosition);
	if (FAILED(hr)) {
		// TODO: Convert HRESULT to POSIX?
		m_lastError = EIO;
		return -1;
	}

	// Truncate the stream.
	ULARGE_INTEGER ulibNewSize;
	ulibNewSize.QuadPart = (uint64_t)size;
	hr = m_pStream->SetSize(ulibNewSize);
	if (FAILED(hr)) {
		// TODO: Convert HRESULT to POSIX?
		m_lastError = EIO;
		return -1;
	}

	// If the previous position was past the new
	// stream size, reset the pointer.
	if (ulibNewPosition.QuadPart > ulibNewSize.QuadPart) {
		dlibMove.QuadPart = size;
		hr = m_pStream->Seek(dlibMove, STREAM_SEEK_SET, nullptr);
		if (FAILED(hr)) {
			// TODO: Convert HRESULT to POSIX?
			m_lastError = EIO;
			return -1;
		}
	}

	// Stream truncated.
	return 0;
}

/** File properties. **/

/**
 * Get the file size.
 * @return File size, or negative on error.
 */
int64_t RpFile_IStream::size(void)
{
	if (!m_pStream) {
		m_lastError = EBADF;
		return -1;
	}

	if (m_pZstm) {
		// zlib-compressed file.
		return static_cast<int64_t>(m_z_uncomp_sz);
	}

	// Use Stat() instead of Seek().
	// TODO: Fallback if Stat() has no size?
	STATSTG statstg;
	HRESULT hr = m_pStream->Stat(&statstg, STATFLAG_NONAME);
	if (FAILED(hr)) {
		// Stat() failed.
		// TODO: Try Seek() instead?
		return -1;
	}

	// TODO: Make sure cbSize is valid?
	return (int64_t)statstg.cbSize.QuadPart;
}

/**
 * Get the filename.
 * @return Filename. (May be empty if the filename is not available.)
 */
string RpFile_IStream::filename(void) const
{
	if (m_filename.empty()) {
		// Get the filename.
		// FIXME: This does NOT have the full path; only the
		// file portion is included. This is enough for the
		// file extension.
		STATSTG statstg;
		HRESULT hr = m_pStream->Stat(&statstg, STATFLAG_DEFAULT);
		if (FAILED(hr)) {
			// Stat() failed.
			return string();
		}

		if (statstg.pwcsName) {
			// Save the filename.
			const_cast<RpFile_IStream*>(this)->m_filename = W2U8(statstg.pwcsName);
			CoTaskMemFree(statstg.pwcsName);
		}
	}

	// Return the filename.
	return m_filename;
}
