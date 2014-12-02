import os
from subprocess import *

def reduce_dir(base, offset, dir):
    '''
    Utility function that uses offset to remove elements from the base
    directory.
    Returns: rewritten base + '/' + dir
    
    Offset should have the form of zero or more .. separated with /
    '''
    up_dirs = len(offset.split('/')) - 1
    if up_dirs > 0:
        base = '/'.join(base.split('/')[0:-up_dirs])  
    return "%s/%s" % (base, dir)


class SvnStandalone(object):
    """
    Functions related to using Subversion from within builds.
    This class does not require the Release heirarchy. 

    This class requires the variable SVN_BINARY to be defined in env.

    Methods pay attention to the env variable 'dry_run'. If dry_run is True,
    commands that make modifications to the repository are only printed.
    """
    def __init__(self, version_object = None):
        self.version_object = version_object


    def run_cmd(self, cmd, dry_run=False):
        if dry_run:
            print "DRY RUN:", cmd
            return (0, "")
        print cmd

        p = Popen(cmd, stdin=None, stdout=PIPE, stderr=STDOUT)
        output = p.communicate()[0]
        status = p.returncode
        return status, output
    
    def tag(self, label, env, project_name=None):
        '''
        Tag the current URL with the given label. Return 1 on error.
        Current directory is where SConstruct is, i.e. top

        Note that this does not tag files changed during the build.

        env requirements:
        * SVN_BINARY must be the path to the svn command
        * SHARED_DIR must be the path to the shared directory parent

        project_name is the name of a directory that is used in the tag.
        If not given, a guess is made of "topdir_project".
        '''
        repo_root = None
        localURL = None # Assume the URL ends in the project name, e.g. hsb
        revision = None

        try:
            dry_run = env['dry_run']
        except KeyError:
            dry_run = 0

        status, output = self.run_cmd([env['SVN_BINARY'], 'info'])
        if status:
            print "Unable to determine local svn information"
            return status
        for line in output.splitlines():
            if line.startswith("Repository Root:"):
                repo_root = line.split()[2].strip()
            if line.startswith("URL:"):
                localURL = line.split()[1].strip()
            if line.startswith("Revision:"):
                revision = line.split()[1].strip()
        if repo_root == None or localURL == None or revision == None:
            print "Unable to determine local svn information from\n%s" % output
            return 1
        baseURL, dir_name = os.path.split(localURL)

        # Examples of localURLs:
        #
        # mote trunk: ... trunk/motes
        # mote branch: ... branches/mote_project/branch_name/motes
        # manager trunk: ... trunk/manager/SMM
        # manager branch: ... branches/manager_project/branch_name/manager/SMM
        #
        # Need to get a project name such as mote_project or manager_project
        # and a branch name if it exists. Tags of branches go in a directory
        # tags/branch_name/tag_name

        if not project_name:
            if baseURL.find("branches") != -1:
                # If the directory is already on a branch, use the existing branch
                # name
                project_name = None
                parts = baseURL.split('/')
                for i, p in enumerate(parts):
                    if p.endswith('_project'):
                        project_name = '/'.join(parts[i:])
                        break
                if not project_name:
                    print "Unable to determine destination for tag from %s" % baseURL
                    return 1
            else:
                # By default, use directoryname_project for the directory in tags
                project_name = "%s_project" % dir_name.lower()
                # Fiddle with some projects' names to get their tags in the right place
                # TODO this might be better passed as a parameter from each caller?
                if project_name in "motes_project":
                    project_name = "mote_project"
                elif project_name in "smm_project":
                    project_name = "manager_project"

        destURL = "%s/tags/%s/%s" % (repo_root, project_name, label)
        if self.tag_exists(destURL, env):
            print "Not tagging source because the tag '%s' has already been used" % (label)
            return 1

        srcURLs = [localURL]
        
        # Make it clearer which paths are involved. Assumes no spaces in paths
        srcURLs_display = '\n'.join(srcURLs)
        print "Start tagging of source directories %s (revision %s)\nwith\n label: %s\n remote URL: %s" % (srcURLs_display, revision, label, destURL)
        cmd = [ env['SVN_BINARY'], "copy", "--parents",
                '-m', "Tagging release build %s" % label,
                '-r', revision ] + srcURLs + [ destURL ]
        #cmd = '%s copy --parents -m "Tagging release build %s" -r %s %s %s ' % (env['SVN_BINARY'], label, revision, srcURLs, destURL)
        status, output = self.run_cmd(cmd, dry_run)
        print output
        print "Finished tagging of source directories %s (revision %s)\nwith\n label: %s\nresult %d" % (srcURLs_display, revision, label, status)
        if status:
            # TODO it turns out that svn copy can fail due to conflicts
            # if another process is using the same destination URL
            # E.g.   svn: Conflict at '/tags/motes'
            # Perhaps just retry?
            return status
        return 0

    def tag_exists(self, destURL, env):
        """
        If the label already exists in SVN for the tagged file, return True.

        env requirements:
        * SVN_BINARY must be the path to the svn command
        """
        status, output = self.run_cmd([env['SVN_BINARY'], 'ls', destURL])
        return not status

    def commit_build_number(self, build_number_file, from_build, env):
        """
        Commit the changes to the build file. Return non-zero on error.

        env requirements:
        * SVN_BINARY   must be the path to the svn command
        """
        try:
            dry_run = env['dry_run']
        except KeyError:
            dry_run = 0

        next_build_number = int(from_build) + 1
        cmd = [ env['SVN_BINARY'], 'commit',
                '-m', "Automatic increment of the build number from %s to %s" % (from_build, next_build_number),
                build_number_file
            ]
        #cmd = "%s commit -m \"Automatic increment of the build number from %s to %s\" %s" % (env['SVN_BINARY'], env['BUILD_NUMBER'], next_build_number, build_number_file)
        status, output = self.run_cmd(cmd, dry_run)
        print output
        return status


    # Scons actions
    
    def tag_and_increment_action(self, target, source, env):
        '''Tag, increment the build number and commit

        The version_object is used to perform the version-related tasks
        '''
        if not self.version_object:
            raise RuntimeError('No version object specified to bump version')

        from_build = self.version_object.get_current_build_number()

        rc = self.version_object.increment_version_action(target, source, env)
        if rc != 0:
            return 1

        label = env['label']
        try:
            project_name = env['project_name']
        except KeyError:
            project_name = None
        rc = self.tag(label, env, project_name)
        if rc != 0:
            return 1

        build_file = self.version_object.get_build_file()
        return self.commit_build_number(build_file, from_build, env)
