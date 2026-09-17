
#ifndef __MAPBUF_HPP__
#define __MAPBUF_HPP__

#include <streambuf>
#include <string>
#include <cstdint>

namespace Zigurat
{

  // A streambuf over a memory-mapped file: the store's alternative to
  // std::basic_filebuf.
  //
  // WHY. A filebuf drops its buffer at every seek, so a store that seeks
  // before every read and every write -- the MVCCS engine, which reads a
  // control block, a hexmap byte or a record with a seek in front of each
  // -- pays a syscall or two per access; a commit of 7000 rows flips 28000
  // control blocks that way. Mapped, a seek is an integer and a read is a
  // memcpy; the kernel's page cache is the buffer and every mapping of the
  // file shares it, so a writer's bytes are visible to a reader's mapping
  // the moment they are written, with no flush between them.
  //
  // EVERY BYTE OF CONTENT GOES THROUGH THE MAPPING, and only the FILE'S
  // LENGTH is moved by a system call. That division is load-bearing and was
  // learnt the expensive way; see "growing" below.
  //
  // THE FILE IS EXACTLY AS LONG AS WHAT WAS WRITTEN at every sync and at
  // close. The engine finds its end by seeking to it (page_count = length /
  // page_size), so a file left longer than its writes would grow phantom
  // pages. Address space is reserved ahead -- RESERVE bytes mapped beyond
  // the file's end, which is legal and costs nothing until touched -- and
  // a fill_n of a page is one write, not eight thousand: the stream above
  // overrides fill_n to write a block.
  //
  // GROWING IS A CHUNK AT A TIME, AND CUT BACK AT EVERY SYNC. A mapped page
  // may not be touched past the file's end, so an extending write has to
  // move the end first -- and moving it is a metadata transaction: 115 us a
  // call on APFS, 4 on ext4. A fresh store page is six extending writes
  // (four of its hexmap slice, two of the page), so at one call per write a
  // store grew at ~700 us a page on APFS and a process writing 128 000 rows
  // spent two thirds of itself inside ftruncate. The file is grown CHUNK
  // bytes ahead instead -- one call per megabyte, ~5 us a page on both
  // systems -- and `sync_to_disk' and `close' cut it back to what was
  // written, so every reader of its length sees the exact one and a clean
  // exit leaves an exact file.
  //
  // WHAT WAS TRIED IN BETWEEN, and why it is not here: making the extending
  // write a pwrite (ZiguratIP c4a7e19). It was faster than one ftruncate a
  // write and kept the length exact at all times, but it put CONTENT
  // through a second path -- write(2) for the bytes past the end, the
  // mapping for everything else -- and a store written that way was
  // intermittently incomplete to the next process that opened it on
  // Linux/ext4 (saman-pasha/ZiguratIP#32: ~30% of first reads, repaired by
  // the open that failed). The mechanism was never established; the mixing
  // was, and it is gone. One path writes the bytes. The kernel is only ever
  // asked to move the end.
  //
  // ONE POSITION, shared by reads and writes, as a filebuf has: the engine
  // was written against that and it stays true here.
  //
  // A READER SEES A WRITER'S GROWTH. A reader's mapbuf learns the file's
  // current length by fstat whenever a read or a seek reaches past the
  // length it knew -- the rare case -- and remaps only if the reservation
  // is outgrown. So per-thread read-only streams over a store another
  // stream is appending to keep answering.
  //
  // Durability is sync_to_disk: msync of the written range, then fsync.
  // POSIX only; the server opens a filestream elsewhere.
  class mapbuf : public std::streambuf
  {
  public:
    // bytes of address space mapped ahead of the file's end
    static const std::size_t RESERVE;
    // bytes the FILE is grown by when a write runs past its end
    static const std::size_t CHUNK;

    mapbuf();
    virtual ~mapbuf();

    mapbuf(const mapbuf&) = delete;
    mapbuf& operator=(const mapbuf&) = delete;

    // in: read only. out: read and write, the file created if missing.
    // trunc: emptied first. Null when the file cannot be opened or mapped.
    mapbuf* open(const std::string& path, std::ios_base::openmode mode);
    mapbuf* close();
    bool is_open() const { return this->_fd >= 0; }
    bool writable() const { return this->_writable; }

    // the file's current length (a reader's view is refreshed first)
    std::streamsize size();
    bool sync_to_disk();

  protected:
    pos_type seekoff(off_type, std::ios_base::seekdir,
                     std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    pos_type seekpos(pos_type,
                     std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    std::streamsize xsgetn(char*, std::streamsize) override;
    std::streamsize xsputn(const char*, std::streamsize) override;
    int_type underflow() override;
    int_type uflow() override;
    int_type overflow(int_type) override;
    int_type pbackfail(int_type) override;
    std::streamsize showmanyc() override;
    int sync() override { return 0; }

  private:
    bool refresh();                    // learn the length another stream may have given the file
    bool reserve(std::size_t);         // map at least this many bytes from offset 0
    bool grow(std::streamsize);        // make the file long enough to write this far (a writer only)
    bool shrink();                     // and cut it back to what was written
    void unmap();

    int _fd;
    char* _base;
    std::size_t _capacity;             // bytes mapped
    std::streamsize _size;             // what has been WRITTEN: the length the engine is promised
    std::streamsize _file;             // what the file is on disk, which may be a chunk ahead
    std::streamsize _pos;              // the one position
    bool _writable;
  };

}

#endif // __MAPBUF_HPP__
