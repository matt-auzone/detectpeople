/**
 * Copyright 2022 by Au-Zone Technologies.  All Rights Reserved.
 *
 * Software that is described herein is for illustrative purposes only which
 * provides customers with programming information regarding the DeepView VAAL
 * library. This software is supplied "AS IS" without any warranties of any
 * kind, and Au-Zone Technologies and its licensor disclaim any and all
 * warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  Au-Zone Technologies assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under
 * any patent, copyright, mask work right, or any other intellectual property
 * rights in or to any products. Au-Zone Technologies reserves the right to make
 * changes in the software without notification. Au-Zone Technologies also makes
 * no representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h>
#endif

#include "vaal.h"

#define USAGE \
    "detect [hv] model.rtm image0 [imageN]\n\
    -h, --help\n\
        Display help information\n\
    -v, --version\n\
        Display version information\n\
    -e, --engine\n\
        Compute engine type \"cpu\", \"npu\"\n\
    -t, --threshold \n\
        Threshold for valid scores, by default it is set to 0.5\n\
    -u, --iou \n\
        IOU threshold for NMS, by default it is set to 0.5\n\
    -n, --norm\n\
        Normalization method applied to input images. \n\
            - raw (default, no processing) \n\
            - unsigned (0...1) \n\
            - signed (-1...1) \n\
            - whitening (per-image standardization/whitening) \n\
            - imagenet (standardization using imagenet) \n\
    -m, --max_detection \n\
        Number of maximum predictions (bounding boxes)\n\
"

int
main(int argc, char* argv[])
{
    // These can be modified as needed
    int         err;
    int         max_detection    = 25; // Max number of boxes to be found
    float       score_thr        = 0.5f; // The score threshold that a box must exceed to be reported
    float       iou_thr          = 0.5f; // The IoU threshold to consider if boxes overlap
    const char* engine           = "npu";
    const char* model            = NULL;
    int         norm             = 0;
    int         max_label        = 16;

    static struct option options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"max_detection", required_argument, NULL, 'm'},
        {"threshold", required_argument, NULL, 't'},
        {"iou", required_argument, NULL, 'u'},
        {"norm", required_argument, NULL, 'n'},
        {"engine", required_argument, NULL, 'e'},
    };

    // Processing of command line arguments
    for (;;) {
        int opt =
            getopt_long(argc, argv, "hv:m:t:u:n:e:", options, NULL);
        if (opt == -1) break;

        switch (opt) {
        case 'h':
            printf(USAGE);
            return EXIT_SUCCESS;
        case 'v':
            printf("DeepView VisionPack Detection Sample with VAAL %s\n",
                   vaal_version(NULL, NULL, NULL, NULL));
            return EXIT_SUCCESS;
        case 'm':
            max_detection = MAX(atoi(optarg), 1);
            break;
        case 't':
            score_thr = CLAMP(atof(optarg), 0.0f, 1.0f);
            break;
        case 'u':
            iou_thr = CLAMP(atof(optarg), 0.0f, 1.0f);
            break;
        case 'n':
            if (strcmp(optarg, "raw") == 0) {
                norm = 0;
            } else if (strcmp(optarg, "signed") == 0) {
                norm = VAAL_IMAGE_PROC_SIGNED_NORM;
            } else if (strcmp(optarg, "unsigned") == 0) {
                norm = VAAL_IMAGE_PROC_UNSIGNED_NORM;
            } else if (strcmp(optarg, "whitening") == 0) {
                norm = VAAL_IMAGE_PROC_WHITENING;
            } else if (strcmp(optarg, "imagenet") == 0) {
                norm = VAAL_IMAGE_PROC_IMAGENET;
            } else {
                fprintf(stderr,
                        "unsupported image normalization method: %s\n",
                        optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'e':
            engine = optarg;
            break;
        default:
            fprintf(stderr,
                    "invalid parameter %c, try --help for usage\n",
                    opt);
            return EXIT_FAILURE;
        }
    }

    if (argv[optind] == NULL) {
        fprintf(stderr, "missing required model, try --help for usage\n");
        return EXIT_FAILURE;
    }

    model = argv[optind++];

    // Initialize boxes object and context with requested engine
    size_t       num_boxes = 0;
    VAALBox*     boxes     = calloc(max_detection, sizeof(VAALBox));
    VAALContext* ctx       = vaal_context_create(engine);

    err = vaal_load_model_file(ctx, model);
    if (err) {
        fprintf(stderr, "failed to load model: %s\n", vaal_strerror(err));
        return EXIT_FAILURE;
    }

    // Set NMS parameters for context, values can be changed at start of main
    vaal_parameter_seti(ctx, "max_detection", &max_detection, 1);
    vaal_parameter_setf(ctx, "score_threshold", &score_thr, 1);
    vaal_parameter_setf(ctx, "iou_threshold", &iou_thr, 1);
    vaal_parameter_seti(ctx, "normalization", &norm, 1);

    printf("  [box] %-*s (scr%%): xmin ymin xmax ymax  [%8s %8s %8s]\r\n",
           max_label,
           "label",
           "load",
           "infer",
           "boxes");

    // Loop through all provided images
    for (int i = optind; i < argc; i++) {
        int64_t     start, load_ns, inference_ns, boxes_ns;
        const char* image = argv[i];

        start = vaal_clock_now();
        // Load image into context
        err   = vaal_load_image_file(ctx, NULL, image, NULL, 0);
        if (err) {
            fprintf(stderr,
                    "failed to load %s: %s\n",
                    image,
                    vaal_strerror(err));
            return EXIT_FAILURE;
        }
        load_ns = vaal_clock_now() - start;

        // Run inference on model
        start        = vaal_clock_now();
        err          = vaal_run_model(ctx);
        inference_ns = vaal_clock_now() - start;
        if (err) {
            fprintf(stderr, "failed to run model: %s\n", vaal_strerror(err));
            return EXIT_FAILURE;
        }

        // Run post-processing and NMS
        start    = vaal_clock_now();
        err      = vaal_boxes(ctx, boxes, max_detection, &num_boxes);
        boxes_ns = vaal_clock_now() - start;
        if (err) {
            fprintf(stderr, "failed to run boxes: %s\n", vaal_strerror(err));
            return EXIT_FAILURE;
        }

        printf("%-52s  [%8.2f %8.2f %8.2f]\n",
               image,
               load_ns / 1e6,
               inference_ns / 1e6,
               boxes_ns / 1e6);

        // Loop through all found boxes and display the results
        for (size_t j = 0; j < num_boxes; j++) {
            char           label_index[12];
            const VAALBox* box   = &boxes[j];
            const char*    label = vaal_label(ctx, box->label);

            if (!label) {
                snprintf(label_index, sizeof(label_index), "%d", box->label);
                label = label_index;
            }

            printf("  [%3zu] %-*s (%3d%%): %3.2f %3.2f %3.2f %3.2f\r\n",
                   j,
                   max_label,
                   label,
                   (int) (box->score * 100),
                   box->xmin,
                   box->ymin,
                   box->xmax,
                   box->ymax);
        }
    }

    // Free memory used for context and boxes
    vaal_context_release(ctx);
    free(boxes);

    return EXIT_SUCCESS;
}
