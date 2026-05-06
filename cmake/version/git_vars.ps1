param([Parameter(Mandatory=$True, ValueFromPipeline=$false)][string]$param_name)

$git_branch=$env:CI_COMMIT_REF_NAME
if (!$git_branch){
    $git_branch=$(git describe --all)
    $git_branch=$git_branch.Replace("tags/", "")
    $git_branch=$git_branch.Replace("heads/", "")
    $git_branch=$git_branch.Replace("remotes/origin/", "")
}
$last_tag=$(git describe --tags --abbrev=0 2>$null)
$commits_since_last_tag=(git log ${last_tag}..HEAD --no-merges --oneline | Measure-Object).Count
$git_describe_output="$git_branch-$commits_since_last_tag-g$(git rev-parse --short HEAD)"
$git_rev_count="$(git rev-list HEAD --count)"

if ($param_name -eq "GIT_VERSION"){
    Write-Output "$git_describe_output"
}

if ($param_name -eq "GIT_COUNT"){
    Write-Output "$git_rev_count"
}

if ($param_name -eq "GIT_BRANCH"){
    Write-Output "$git_branch"
}
