#include "face_engine.h"
#include "v4l2_capture.h"
#include "rpmsg_client.h"
#include "access_control.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>

#include <opencv2/imgproc.hpp>

static const float kMatchThreshold = 0.55f;

struct RegFace {
    std::string name;
    std::vector<float> feat;
};

static bool load_feat(const std::string& path, std::vector<float>& feat)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    float buf[512];
    size_t n = fread(buf, sizeof(float), 512, f);
    fclose(f);
    if (n != 512) return false;
    feat.assign(buf, buf + 512);
    l2_normalize(feat);
    return true;
}

static bool save_feat(const std::string& path, const std::vector<float>& feat)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(feat.data(), sizeof(float), feat.size(), f);
    fclose(f);
    return true;
}

static std::vector<RegFace> load_db(const std::string& dir)
{
    std::vector<RegFace> db;
    DIR* d = opendir(dir.c_str());
    if (!d) return db;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        std::string name = e->d_name;
        if (name.size() < 4 || name.substr(name.size() - 4) != ".bin")
            continue;
        std::vector<float> feat;
        std::string path = dir + "/" + name;
        if (!load_feat(path, feat))
            continue;
        RegFace rf;
        rf.name = name.substr(0, name.size() - 4);
        rf.feat = feat;
        db.push_back(rf);
    }
    closedir(d);
    return db;
}

static void usage(const char* prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s verify <detect.param> <detect.bin> <recog.param> <recog.bin> <faces_dir>\n"
        "  %s register <name> <detect.param> <detect.bin> <recog.param> <recog.bin> <faces_dir>\n",
        prog, prog);
}

// Find the best-matching registered face. Returns index or -1.
static int match_face(const std::vector<float>& feat,
                      const std::vector<RegFace>& db, float& best_sim)
{
    int best = -1;
    best_sim = 0.f;
    for (size_t i = 0; i < db.size(); i++) {
        float sim = cosine_similarity(feat, db[i].feat);
        if (sim > best_sim) {
            best_sim = sim;
            best = (int)i;
        }
    }
    return best;
}

static int run_register(const std::string& name, const std::string& faces_dir,
                        FaceEngine& engine, V4L2Capture& cam)
{
    // capture a few frames and use the largest detected face
    for (int attempt = 0; attempt < 5; attempt++) {
        cv::Mat bgr;
        if (!cam.read(bgr)) continue;
        std::vector<FaceBox> faces = engine.detect(bgr);
        if (faces.empty()) {
            usleep(200000);
            continue;
        }
        // pick the face with largest area
        FaceBox* best = &faces[0];
        float best_area = -1.f;
        for (auto& f : faces) {
            float area = (f.x2 - f.x1) * (f.y2 - f.y1);
            if (area > best_area) { best_area = area; best = &f; }
        }
        std::vector<float> feat = engine.extract(bgr, *best);
        if (feat.empty()) continue;

        std::string path = faces_dir + "/" + name + ".bin";
        if (!save_feat(path, feat)) {
            fprintf(stderr, "failed to save feature to %s\n", path.c_str());
            return 1;
        }
        printf("registered face '%s' -> %s (%zu dims)\n",
               name.c_str(), path.c_str(), feat.size());
        return 0;
    }
    fprintf(stderr, "no face detected for registration\n");
    return 1;
}

static int run_verify(FaceEngine& engine, V4L2Capture& cam,
                      RPMsgClient& rpmsg, const std::vector<RegFace>& db)
{
    printf("access control running: %zu registered face(s)\n", db.size());

    for (;;) {
        // wait for PERSON_DETECTED from M4
        uint32_t msg_id = 0, value = 0;
        if (!rpmsg.recv(msg_id, value, 2000))
            continue;
        if (msg_id != AC_MSG_PERSON_DETECTED)
            continue;

        printf("person detected, running recognition...\n");

        cv::Mat bgr;
        if (!cam.read(bgr)) {
            fprintf(stderr, "camera read failed\n");
            continue;
        }

        std::vector<FaceBox> faces = engine.detect(bgr);
        if (faces.empty()) {
            printf("no face found -> AUTH_FAILED\n");
            rpmsg.send(AC_MSG_AUTH_FAILED, 0);
            continue;
        }

        bool authorized = false;
        std::string who = "unknown";
        for (auto& f : faces) {
            std::vector<float> feat = engine.extract(bgr, f);
            if (feat.empty()) continue;
            float sim = 0.f;
            int idx = match_face(feat, db, sim);
            if (idx >= 0 && sim > kMatchThreshold) {
                authorized = true;
                who = db[idx].name;
                printf("MATCH: %s (sim=%.3f)\n", who.c_str(), sim);
                break;
            }
        }

        if (authorized) {
            printf("authorized -> AUTH_SUCCESS + UNLOCK\n");
            rpmsg.send(AC_MSG_AUTH_SUCCESS, 0);
            rpmsg.send(AC_MSG_UNLOCK, 0);
        } else {
            printf("not authorized -> AUTH_FAILED\n");
            rpmsg.send(AC_MSG_AUTH_FAILED, 0);
        }
    }

    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 7) { usage(argv[0]); return 1; }

    std::string mode = argv[1];
    if (mode != "verify" && mode != "register") { usage(argv[0]); return 1; }

    std::string detect_param, detect_bin, recog_param, recog_bin, faces_dir;
    std::string reg_name;

    if (mode == "verify") {
        if (argc != 7) { usage(argv[0]); return 1; }
        detect_param = argv[2];
        detect_bin   = argv[3];
        recog_param  = argv[4];
        recog_bin    = argv[5];
        faces_dir    = argv[6];
    } else {
        if (argc != 8) { usage(argv[0]); return 1; }
        reg_name     = argv[2];
        detect_param = argv[3];
        detect_bin   = argv[4];
        recog_param  = argv[5];
        recog_bin    = argv[6];
        faces_dir    = argv[7];
    }

    FaceEngine engine;
    if (!engine.init(detect_param, detect_bin, recog_param, recog_bin)) {
        fprintf(stderr, "failed to load models\n");
        return 1;
    }
    printf("models loaded\n");

    V4L2Capture cam;
    if (!cam.open("/dev/video0", 640, 480)) {
        fprintf(stderr, "failed to open camera\n");
        return 1;
    }
    printf("camera opened: %dx%d\n", cam.width(), cam.height());

    if (mode == "register")
        return run_register(reg_name, faces_dir, engine, cam);

    // verify mode: load DB + open rpmsg
    std::vector<RegFace> db = load_db(faces_dir);

    RPMsgClient rpmsg;
    if (!rpmsg.open("/dev/access_control")) {
        fprintf(stderr, "failed to open /dev/access_control\n");
        return 1;
    }
    printf("rpmsg client opened\n");

    return run_verify(engine, cam, rpmsg, db);
}
