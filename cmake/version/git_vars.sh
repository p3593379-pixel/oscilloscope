#!/bin/bash

git_branch=${CI_COMMIT_REF_NAME}

branch_name() {
    if [[ -z "${git_branch}" ]]
    then
        git_branch=$(git describe --all | sed -e 's:remotes/origin/::' -e 's:heads/::' -e 's:tags/::')
    fi
    echo "${git_branch}"
}

git_count() {
    git rev-list HEAD --count
}

version() {
    branch=$(branch_name);
    last_tag=$(git describe --tags --abbrev=0 2>/dev/null)
    commits_since_last_tag=$(git log ${last_tag}..HEAD --no-merges --oneline | wc -l)
    echo "${branch}-${commits_since_last_tag}-g$(git rev-parse --short HEAD)"
}

case "$1" in
  GIT_VERSION)
    version
    ;;
  GIT_COUNT)
    git_count
    ;;
  GIT_BRANCH)
    branch_name
    ;;
  *)
    echo "Usage: $0 {GIT_VERSION|GIT_COUNT|GIT_BRANCH}"
esac
