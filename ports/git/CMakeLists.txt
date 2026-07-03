set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

file(MAKE_DIRECTORY ${GEN}/compat)

function(git_patch relpath)
    add_custom_command(
        OUTPUT ${GEN}/${relpath}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/${relpath} ${GEN}/${relpath}
        COMMAND sed -z -i ${ARGN} ${GEN}/${relpath}
        DEPENDS repo/${relpath}
        VERBATIM
    )
endfunction()

git_patch(compat/posix.h
    -e [[s|#if defined(WIN32) && !defined(__CYGWIN__)|#if defined(WIN32) \&\& !defined(__CYGWIN__) \&\& !defined(MOXNOX)|g]]
    -e [[s|#include <utime.h>|#include <utime.h>\n#ifdef MOXNOX\n#include <arpa/inet.h>\n#endif|g]]
)

git_patch(git-compat-util.h
    -e [[s|#if defined(__MINGW32__)|#if defined(__MINGW32__) \&\& !defined(MOXNOX)|g]]
)

add_custom_target(git_generate DEPENDS
    ${GEN}/compat/posix.h
    ${GEN}/git-compat-util.h
)

add_custom_command(
    OUTPUT ${GEN}/run-command.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/run-command.c ${GEN}/run-command.c
    COMMAND patch ${GEN}/run-command.c
            ${CMAKE_CURRENT_SOURCE_DIR}/patches/run-command-posix-spawn.patch
    DEPENDS repo/run-command.c
            ${CMAKE_CURRENT_SOURCE_DIR}/patches/run-command-posix-spawn.patch
    VERBATIM
)

add_library(libgit STATIC
    repo/abspath.c
    repo/add-interactive.c
    repo/add-patch.c
    repo/advice.c
    repo/alias.c
    repo/alloc.c
    repo/apply.c
    repo/archive-tar.c
    repo/archive-zip.c
    repo/archive.c
    repo/attr.c
    repo/base85.c
    repo/bisect.c
    repo/blame.c
    repo/blob.c
    repo/bloom.c
    repo/branch.c
    repo/bundle-uri.c
    repo/bundle.c
    repo/cache-tree.c
    repo/cbtree.c
    repo/chdir-notify.c
    repo/checkout.c
    repo/chunk-format.c
    repo/color.c
    repo/column.c
    repo/combine-diff.c
    repo/commit-graph.c
    repo/commit-reach.c
    repo/commit.c
    repo/common-exit.c
    repo/common-init.c
    repo/common-main.c
    repo/compat/nonblock.c
    repo/compat/obstack.c
    repo/compat/open.c
    repo/compat/terminal.c
    repo/compiler-tricks/not-constant.c
    repo/config.c
    repo/connect.c
    repo/connected.c
    repo/convert.c
    repo/copy.c
    repo/credential.c
    repo/csum-file.c
    repo/ctype.c
    repo/date.c
    repo/decorate.c
    repo/delta-islands.c
    repo/diagnose.c
    repo/diff-delta.c
    repo/diff-merges.c
    repo/diff-lib.c
    repo/diff-no-index.c
    repo/diff.c
    repo/diffcore-break.c
    repo/diffcore-delta.c
    repo/diffcore-order.c
    repo/diffcore-pickaxe.c
    repo/diffcore-rename.c
    repo/diffcore-rotate.c
    repo/dir-iterator.c
    repo/dir.c
    repo/editor.c
    repo/entry.c
    repo/environment.c
    repo/ewah/bitmap.c
    repo/ewah/ewah_bitmap.c
    repo/ewah/ewah_io.c
    repo/ewah/ewah_rlw.c
    repo/exec-cmd.c
    repo/fetch-negotiator.c
    repo/fetch-pack.c
    repo/fmt-merge-msg.c
    repo/fsck.c
    repo/fsmonitor.c
    repo/fsmonitor-ipc.c
    repo/fsmonitor-settings.c
    repo/gettext.c
    repo/git-zlib.c
    repo/gpg-interface.c
    repo/graph.c
    repo/grep.c
    repo/hash-lookup.c
    repo/hash.c
    repo/hashmap.c
    repo/help.c
    repo/hex.c
    repo/hex-ll.c
    repo/hook.c
    repo/ident.c
    repo/json-writer.c
    repo/kwset.c
    repo/levenshtein.c
    repo/line-log.c
    repo/line-range.c
    repo/linear-assignment.c
    repo/list-objects-filter-options.c
    repo/list-objects-filter.c
    repo/list-objects.c
    repo/lockfile.c
    repo/log-tree.c
    repo/loose.c
    repo/ls-refs.c
    repo/mailinfo.c
    repo/mailmap.c
    repo/match-trees.c
    repo/mem-pool.c
    repo/merge-blobs.c
    repo/merge-ll.c
    repo/merge-ort.c
    repo/merge-ort-wrappers.c
    repo/merge.c
    repo/midx.c
    repo/midx-write.c
    repo/name-hash.c
    repo/negotiator/default.c
    repo/negotiator/noop.c
    repo/negotiator/skipping.c
    repo/notes-cache.c
    repo/notes-merge.c
    repo/notes-utils.c
    repo/notes.c
    repo/object-file-convert.c
    repo/object-file.c
    repo/object-name.c
    repo/object.c
    repo/odb.c
    repo/odb/streaming.c
    repo/oid-array.c
    repo/oidmap.c
    repo/oidset.c
    repo/oidtree.c
    repo/pack-bitmap-write.c
    repo/pack-bitmap.c
    repo/pack-check.c
    repo/pack-mtimes.c
    repo/pack-objects.c
    repo/pack-refs.c
    repo/pack-revindex.c
    repo/pack-write.c
    repo/packfile.c
    repo/pager.c
    repo/parallel-checkout.c
    repo/parse.c
    repo/parse-options-cb.c
    repo/parse-options.c
    repo/patch-delta.c
    repo/patch-ids.c
    repo/path.c
    repo/path-walk.c
    repo/pathspec.c
    repo/pkt-line.c
    repo/preload-index.c
    repo/pretty.c
    repo/prio-queue.c
    repo/progress.c
    repo/promisor-remote.c
    repo/prompt.c
    repo/protocol.c
    repo/protocol-caps.c
    repo/prune-packed.c
    repo/pseudo-merge.c
    repo/quote.c
    repo/range-diff.c
    repo/reachable.c
    repo/read-cache.c
    repo/rebase-interactive.c
    repo/rebase.c
    repo/ref-filter.c
    repo/reflog-walk.c
    repo/reflog.c
    repo/refs.c
    repo/refs/debug.c
    repo/refs/files-backend.c
    repo/refs/reftable-backend.c
    repo/refs/iterator.c
    repo/refs/packed-backend.c
    repo/refs/ref-cache.c
    repo/refspec.c
    repo/reftable/basics.c
    repo/reftable/block.c
    repo/reftable/blocksource.c
    repo/reftable/error.c
    repo/reftable/fsck.c
    repo/reftable/iter.c
    repo/reftable/merged.c
    repo/reftable/pq.c
    repo/reftable/record.c
    repo/reftable/stack.c
    repo/reftable/system.c
    repo/reftable/table.c
    repo/reftable/tree.c
    repo/reftable/writer.c
    repo/remote.c
    repo/repack.c
    repo/repack-cruft.c
    repo/repack-filtered.c
    repo/repack-geometry.c
    repo/repack-midx.c
    repo/repack-promisor.c
    repo/replace-object.c
    repo/repo-settings.c
    repo/repository.c
    repo/rerere.c
    repo/reset.c
    repo/resolve-undo.c
    repo/revision.c
    ${GEN}/run-command.c
    repo/send-pack.c
    repo/sequencer.c
    repo/serve.c
    repo/server-info.c
    repo/setup.c
    repo/shallow.c
    repo/sideband.c
    repo/sigchain.c
    repo/sparse-index.c
    repo/split-index.c
    repo/stable-qsort.c
    repo/statinfo.c
    repo/strbuf.c
    repo/string-list.c
    repo/strmap.c
    repo/strvec.c
    repo/sub-process.c
    repo/submodule-config.c
    repo/submodule.c
    repo/symlinks.c
    repo/tag.c
    repo/tempfile.c
    repo/thread-utils.c
    repo/tmp-objdir.c
    repo/trace.c
    repo/trace2.c
    repo/trace2/tr2_cfg.c
    repo/trace2/tr2_cmd_name.c
    repo/trace2/tr2_ctr.c
    repo/trace2/tr2_dst.c
    repo/trace2/tr2_sid.c
    repo/trace2/tr2_sysenv.c
    repo/trace2/tr2_tbuf.c
    repo/trace2/tr2_tgt_event.c
    repo/trace2/tr2_tgt_normal.c
    repo/trace2/tr2_tgt_perf.c
    repo/trace2/tr2_tls.c
    repo/trace2/tr2_tmr.c
    repo/trailer.c
    repo/transport-helper.c
    repo/transport.c
    repo/tree-diff.c
    repo/tree-walk.c
    repo/tree.c
    repo/unpack-trees.c
    repo/upload-pack.c
    repo/url.c
    repo/urlmatch.c
    repo/usage.c
    repo/userdiff.c
    repo/utf8.c
    repo/varint.c
    repo/version.c
    repo/versioncmp.c
    repo/walker.c
    repo/wildmatch.c
    repo/worktree.c
    repo/wrapper.c
    repo/write-or-die.c
    repo/ws.c
    repo/wt-status.c
    repo/xdiff-interface.c
    repo/xdiff/xdiffi.c
    repo/xdiff/xemit.c
    repo/xdiff/xhistogram.c
    repo/xdiff/xmerge.c
    repo/xdiff/xpatience.c
    repo/xdiff/xprepare.c
    repo/xdiff/xutils.c
    repo/sha1dc_git.c
    repo/sha1dc/sha1.c
    repo/sha1dc/ubc_check.c
    repo/sha256/block/sha256.c
    repo/compat/strcasestr.c
    repo/compat/strlcpy.c
    repo/compat/memmem.c
    repo/compat/pread.c
    repo/compat/regex/regex.c
    repo/builtin/add.c
    repo/builtin/am.c
    repo/builtin/annotate.c
    repo/builtin/apply.c
    repo/builtin/archive.c
    repo/builtin/backfill.c
    repo/builtin/bisect.c
    repo/builtin/blame.c
    repo/builtin/branch.c
    repo/builtin/bugreport.c
    repo/builtin/bundle.c
    repo/builtin/cat-file.c
    repo/builtin/check-attr.c
    repo/builtin/check-ignore.c
    repo/builtin/check-mailmap.c
    repo/builtin/check-ref-format.c
    repo/builtin/checkout--worker.c
    repo/builtin/checkout-index.c
    repo/builtin/checkout.c
    repo/builtin/clean.c
    repo/builtin/clone.c
    repo/builtin/column.c
    repo/builtin/commit-graph.c
    repo/builtin/commit-tree.c
    repo/builtin/commit.c
    repo/builtin/config.c
    repo/builtin/count-objects.c
    repo/builtin/credential-cache--daemon.c
    repo/builtin/credential-cache.c
    repo/builtin/credential-store.c
    repo/builtin/credential.c
    repo/builtin/describe.c
    repo/builtin/diagnose.c
    repo/builtin/diff-files.c
    repo/builtin/diff-index.c
    repo/builtin/diff-pairs.c
    repo/builtin/diff-tree.c
    repo/builtin/diff.c
    repo/builtin/difftool.c
    repo/builtin/fast-export.c
    repo/builtin/fast-import.c
    repo/builtin/fetch-pack.c
    repo/builtin/fetch.c
    repo/builtin/fmt-merge-msg.c
    repo/builtin/for-each-ref.c
    repo/builtin/for-each-repo.c
    repo/builtin/fsck.c
    repo/builtin/fsmonitor--daemon.c
    repo/builtin/gc.c
    repo/builtin/get-tar-commit-id.c
    repo/builtin/grep.c
    repo/builtin/hash-object.c
    repo/builtin/help.c
    repo/builtin/hook.c
    repo/builtin/index-pack.c
    repo/builtin/init-db.c
    repo/builtin/interpret-trailers.c
    repo/builtin/last-modified.c
    repo/builtin/log.c
    repo/builtin/ls-files.c
    repo/builtin/ls-remote.c
    repo/builtin/ls-tree.c
    repo/builtin/mailinfo.c
    repo/builtin/mailsplit.c
    repo/builtin/merge-base.c
    repo/builtin/merge-file.c
    repo/builtin/merge-index.c
    repo/builtin/merge-ours.c
    repo/builtin/merge-recursive.c
    repo/builtin/merge-tree.c
    repo/builtin/merge.c
    repo/builtin/mktag.c
    repo/builtin/mktree.c
    repo/builtin/multi-pack-index.c
    repo/builtin/mv.c
    repo/builtin/name-rev.c
    repo/builtin/notes.c
    repo/builtin/pack-objects.c
    repo/builtin/pack-redundant.c
    repo/builtin/pack-refs.c
    repo/builtin/patch-id.c
    repo/builtin/prune-packed.c
    repo/builtin/prune.c
    repo/builtin/pull.c
    repo/builtin/push.c
    repo/builtin/range-diff.c
    repo/builtin/read-tree.c
    repo/builtin/rebase.c
    repo/builtin/receive-pack.c
    repo/builtin/reflog.c
    repo/builtin/refs.c
    repo/builtin/remote-ext.c
    repo/builtin/remote-fd.c
    repo/builtin/remote.c
    repo/builtin/repack.c
    repo/builtin/replace.c
    repo/builtin/replay.c
    repo/builtin/repo.c
    repo/builtin/rerere.c
    repo/builtin/reset.c
    repo/builtin/rev-list.c
    repo/builtin/rev-parse.c
    repo/builtin/revert.c
    repo/builtin/rm.c
    repo/builtin/send-pack.c
    repo/builtin/shortlog.c
    repo/builtin/show-branch.c
    repo/builtin/show-index.c
    repo/builtin/show-ref.c
    repo/builtin/sparse-checkout.c
    repo/builtin/stash.c
    repo/builtin/stripspace.c
    repo/builtin/submodule--helper.c
    repo/builtin/symbolic-ref.c
    repo/builtin/tag.c
    repo/builtin/unpack-file.c
    repo/builtin/unpack-objects.c
    repo/builtin/update-index.c
    repo/builtin/update-ref.c
    repo/builtin/update-server-info.c
    repo/builtin/upload-archive.c
    repo/builtin/upload-pack.c
    repo/builtin/var.c
    repo/builtin/verify-commit.c
    repo/builtin/verify-pack.c
    repo/builtin/verify-tag.c
    repo/builtin/worktree.c
    repo/builtin/write-tree.c
    repo/compat/stub/procinfo.c
    repo/compat/qsort_s.c
    ${CMAKE_CURRENT_SOURCE_DIR}/moxnox-compat.c
)

target_include_directories(libgit PUBLIC
    ${GEN}
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/compat/regex
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)

target_compile_definitions(libgit PUBLIC
    WIN32
    _CRTIMP=
    NOGDI
    "STRIP_EXTENSION=\".exe\""
    NO_SYS_POLL_H
    NO_STRCASESTR
    NO_STRLCPY
    NO_MEMMEM
    NO_MKDTEMP
    NO_PREAD
    NO_REGEX # FIXME
    NO_MBSUPPORT
    GAWK
    NO_POSIX_GOODIES
    NO_UNIX_SOCKETS
    NO_ST_BLOCKS_IN_STRUCT_STAT
    NO_SYMLINK_HEAD
    NO_ICONV
    NO_GETTEXT
    NO_OPENSSL
    OBJECT_CREATION_MODE=1
    HAVE_MMAP
    NO_SETITIMER
    NO_NSEC
    HAVE_PTHREADS
    HAVE_GETENTROPY
    HAVE_STRINGS_H
    COMPAT_MINGW_POSIX_H
    NO_STRUCT_ITIMERVAL
    HAVE_DEV_TTY
    SHA1_DC
    SHA1DC_NO_STANDARD_INCLUDES
    SHA1DC_INIT_SAFE_HASH_DEFAULT=0
    "SHA1DC_CUSTOM_INCLUDE_SHA1_C=\"git-compat-util.h\""
    "SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C=\"git-compat-util.h\""
    SHA256_BLK
    INTERNAL_QSORT
    NO_GECOS_IN_PWENT
    "FALLBACK_RUNTIME_PREFIX=\"/\""
    "GIT_HTML_PATH=\"share/doc/git-doc\""
    "GIT_MAN_PATH=\"share/man\""
    "GIT_INFO_PATH=\"share/info\""
    "GIT_EXEC_PATH=\"bin/git-core\""
    "ETC_GITCONFIG=\"etc/gitconfig\""
    "ETC_GITATTRIBUTES=\"etc/gitattributes\""
    "GIT_HOST_CPU=\"x86_64\""
    "PAGER_ENV=\"LESS=FRX LV=-c\""
    "DEFAULT_GIT_TEMPLATE_DIR=\"share/git-core/templates\""
)

target_compile_options(libgit PUBLIC
    ${USERSPACE_CFLAGS}
    -include "${CMAKE_CURRENT_SOURCE_DIR}/moxnox-git.h"
    -Wno-conversion
    -Wno-sign-compare
    -Wno-unused-function
    -Wno-empty-body
    -Wno-missing-field-initializers
    -Wno-macro-redefined
)

target_link_libraries(libgit PUBLIC zlib moxnox libcurl)

add_dependencies(libgit git_generate)

add_executable(git repo/git.c)
target_link_libraries(git PRIVATE libgit)

add_executable(git-remote-http
    repo/remote-curl.c
    repo/http.c
    repo/http-walker.c
)
target_compile_definitions(git-remote-http PRIVATE NO_EXPAT)
target_link_libraries(git-remote-http PRIVATE libgit)
add_dependencies(git-remote-http git_generate)

set(GIT_TEMPLATE_FILES
    description
    hooks/applypatch-msg.sample
    hooks/commit-msg.sample
    hooks/fsmonitor-watchman.sample
    hooks/post-update.sample
    hooks/pre-applypatch.sample
    hooks/pre-commit.sample
    hooks/pre-merge-commit.sample
    hooks/prepare-commit-msg.sample
    hooks/pre-push.sample
    hooks/pre-rebase.sample
    hooks/pre-receive.sample
    hooks/push-to-checkout.sample
    hooks/sendemail-validate.sample
    hooks/update.sample
    info/exclude
)

set(GIT_TEMPLATE_OUTPUTS)
foreach(t ${GIT_TEMPLATE_FILES})
    get_filename_component(_tdir ${t} DIRECTORY)
    add_custom_command(
        OUTPUT ${GEN}/templates/${t}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}/templates/${_tdir}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/templates/${t} ${GEN}/templates/${t}
        COMMAND sed -i -e [[1s|#!.*/sh|#!/bin/sh|]] -e [[s|@SHELL_PATH@|/bin/sh|g]] -e [[s|@PERL_PATH@|perl|g]] ${GEN}/templates/${t}
        DEPENDS repo/templates/${t}
        VERBATIM
    )
    list(APPEND GIT_TEMPLATE_OUTPUTS ${GEN}/templates/${t})
endforeach()
add_custom_target(git_templates DEPENDS ${GIT_TEMPLATE_OUTPUTS})

add_dependencies(git git_generate git_templates)

install(TARGETS git DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(DIRECTORY ${GEN}/templates/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/git-core/templates
        USE_SOURCE_PERMISSIONS)

install(TARGETS git-remote-http DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/git-core)
install(CODE "file(CREATE_LINK git-remote-http.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/git-core/git-remote-https.exe\" SYMBOLIC)")
