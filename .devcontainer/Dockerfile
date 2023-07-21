# if you get error: 
# "pull access denied for sha256, repository does not exist or may require 'docker login' : denied: requested access to the resource is denied"
# when opening the project, this may mean that base SDK container has been deleted 
# or that the project has been moved to a different machine where such container does not exist, 
# you can re-build it by pressing F1 in Visual Studio Code and selecting:
# "Torizon: Rebuild SDK and reload in container"
# after this operation you should be able to re-open your project inside a container
FROM detectimg_arm64v8-debian-no-ssh_bullseye_debug_7be98ced-f527-411c-8032-ff1022273bfb_sdk_image

# This is required to allow regular user inside the container to access docker socket
RUN addgroup docker && groupmod --non-unique --gid 0 docker && adduser torizon docker
