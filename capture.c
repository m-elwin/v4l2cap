#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define VIDEO_DEVICE "/dev/video0"

int main(void)
{
    printf("Opening Device "VIDEO_DEVICE".\n");

    int fd = open(VIDEO_DEVICE, O_RDWR);
    if(-1 == fd)
    {
        perror("Error Opening Video Device "VIDEO_DEVICE".");
        exit(EXIT_FAILURE);
    }

    printf("\nChecking device capabilities.\n");

    // https://docs.kernel.org/userspace-api/media/v4l/vidioc-querycap.html#vidioc-querycap
    struct v4l2_capability caps = {0};
    if(-1 == ioctl(fd, VIDIOC_QUERYCAP, &caps))
    {
        perror("Error Querying Capabilities");
        exit(EXIT_FAILURE);
    }
    printf("Driver: %s\nCard %s\nBus: %s\nVersion: %d\n",
           caps.driver,
           caps.card,
           caps.bus_info,
           caps.version
        );
    // Other capabilities are in the caps.capabilities field. We mainly care about video capture
    printf("Capabilities: 0x%x, Video Capture Bit: %d\n",
           caps.capabilities,
           caps.capabilities & V4L2_CAP_VIDEO_CAPTURE);

    printf("\nGetting Video Formats\n");
    // https://docs.kernel.org/userspace-api/media/v4l/vidioc-g-fmt.html#vidioc-g-fmt
    struct v4l2_format format = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    if(-1 == ioctl(fd, VIDIOC_G_FMT, &format))
    {
        perror("Error Querying Formats.");
        exit(EXIT_FAILURE);
    }

    printf("\tWidth: %d Height %d\n", format.fmt.pix.width, format.fmt.pix.height);
    char pixelformat[5]="";
    pixelformat[3] = format.fmt.pix.pixelformat >> 24;
    pixelformat[2] = (format.fmt.pix.pixelformat & 0x00FF0000) >> 16;
    pixelformat[1] = (format.fmt.pix.pixelformat & 0x0000FF00) >> 8;
    pixelformat[0] = format.fmt.pix.pixelformat & 0x000000FF;
    printf("\tPixel Format: %s\n", pixelformat);

    // Rather than setting a format we will use the one the camera is currently set to

    // The next step is to request that the device allocate a buffer for streaming
    printf("\nRequesting Buffer for MMAP Streaming\n");
    // There are other streaming methods to try but
    // mmap streaming seems to be the most common
    // https://docs.kernel.org/userspace-api/media/v4l/vidioc-reqbufs.html#vidioc-reqbufs
    struct v4l2_requestbuffers req =
        {
            .count = 1,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
    if(-1 == ioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Error Requesting Buffers.");
        exit(EXIT_FAILURE);
    }

    // map the buffer into memory so this process can access it
    struct v4l2_buffer buf =
        {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index = 0
        };
    if(-1 == ioctl(fd, VIDIOC_QUERYBUF, & buf))
    {
        perror("Error Querying Buffer");
        exit(EXIT_FAILURE);
    }
    printf("Buffer size is %d\n", buf.length);

    printf("Allocated the memory-mapped buffer\n");
    u_int8_t * buffer = NULL;
    buffer = (u_int8_t*)mmap(NULL,
                             buf.length,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             fd,
                             buf.m.offset);

    if(MAP_FAILED == buffer)
    {
        perror("Error mapping memory");
        exit(EXIT_FAILURE);
    }

    printf("Beginning the stream\n");
    if(-1 == ioctl(fd, VIDIOC_STREAMON, &buf.type))
    {
        perror("Could not begin stream.");
        exit(EXIT_FAILURE);
    }

    // Grab a frame.
    // Wait for a frame to be available
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval timeout = { .tv_sec = 2 };
    if( -1 == select(fd + 1, &fds, NULL, NULL, &timeout))
    {
        perror("Error waiting for frame.");
        exit(EXIT_FAILURE);
    }

    printf("Grabbing the frame\n");
    // Grab the frame
    if(-1 == ioctl(fd, VIDIOC_DQBUF, &buf))
    {
        perror("Error grabbing frame.");
        exit(EXIT_FAILURE);
    }


    printf("Creating temporary output file\n");
    // write the frame to a temporary file
    char tmpfile_name[]="video-XXXXXX";
    int tmpfile = mkstemp(tmpfile_name);
    if(-1 == tmpfile )
    {
        perror("Error creating temporary output file.");
        exit(EXIT_FAILURE);
    }
    printf("Writing frame to %s\n", tmpfile_name);
    if( -1 == write(tmpfile, buffer, buf.length))
    {
        perror("Error writing to tmpfile.");
        exit(EXIT_FAILURE);
    }
    printf("Frame capture complete.\n");
    return 0;
}
