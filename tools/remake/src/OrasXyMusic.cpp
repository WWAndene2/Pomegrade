#include "OrasXyMusic.h"

#include "Bcsar.h"
#include "Bcstm.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <future>
#include <mutex>
#include <thread>

namespace remake
{

namespace
{
constexpr uint32_t Rate = 32728;

// a whole command's standard output
Bytes RunCapture(const std::string& command)
{
    FILE* p = popen(command.c_str(), "r");
    if (!p) throw FormatError("could not run: " + command);
    Bytes out;
    uint8_t buffer[1 << 16];
    for (size_t n; (n = fread(buffer, 1, sizeof buffer, p)) > 0;) out.insert(out.end(), buffer, buffer + n);
    if (pclose(p) != 0) throw FormatError("failed: " + command);
    return out;
}

std::string Quoted(const std::string& path) { return "'" + path + "'"; }

// one of X/Y's streams as Omega Ruby's BCSTM
Bytes ConvertStream(const Bytes& aac, const BcsarStream& s, const std::string& scratch, std::string& note)
{
    const std::string file = scratch + "/" + s.File;
    WriteFile(file, aac);
    const Bytes rateText = RunCapture("ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate -of csv=p=0 " + Quoted(file));
    const double sourceRate = std::stod(std::string(rateText.begin(), rateText.end()));
    const Bytes pcm = RunCapture("ffmpeg -v error -i " + Quoted(file) + " -f s16le -ac 2 -ar " + std::to_string(Rate) + " -");
    std::filesystem::remove(file);
    StreamAudio a;
    a.Rate = Rate;
    a.Channels.assign(2, {});
    for (size_t at = 0; at + 4 <= pcm.size(); at += 4)
    {
        a.Channels[0].push_back((int16_t)(pcm[at] | pcm[at + 1] << 8));
        a.Channels[1].push_back((int16_t)(pcm[at + 2] | pcm[at + 3] << 8));
    }
    if (a.Channels[0].empty()) throw FormatError(s.File + ": no samples decoded");
    if (s.HasLoop && s.LoopEnd > s.LoopStart)
    {
        const size_t end = std::min(a.Channels[0].size(), (size_t)std::llround(s.LoopEnd * (double)Rate / sourceRate));
        for (auto& c : a.Channels) c.resize(end);
        a.Loops = true;
        a.LoopStart = (uint32_t)std::llround(s.LoopStart * (double)Rate / sourceRate);
        if (a.LoopStart >= end) a.Loops = false, a.LoopStart = 0;
    }
    uint32_t pad = 0;
    const Bytes bcstm = WriteBcstm(a, &pad);
    char line[200];
    snprintf(line, sizeof line, "%s: %.1f s%s, %zu bytes", s.File.c_str(), a.Channels[0].size() / (double)Rate,
             a.Loops ? (", loop from " + std::to_string(a.LoopStart / (double)Rate).substr(0, 5) + " s, " + std::to_string(pad) + " samples of silence before").c_str() : ", no loop",
             bcstm.size());
    note = line;
    return bcstm;
}
}

std::vector<std::string> BuildXyMusic(N3dsRom& oras, N3dsRom& xy, const XySounds& sounds, const std::string& outDir, const std::string& scratchDir,
                                      Bytes& archive)
{
    std::vector<std::string> log;
    // the template: Omega Ruby's STRM_BGM_C_01 (a stereo stream of one track, as every one of X/Y's)
    size_t like = SIZE_MAX;
    for (const BcsarStream& s : BcsarStreams(archive)) if (s.Name == "STRM_BGM_C_01") like = s.Index;
    if (like == SIZE_MAX) throw FormatError("Omega Ruby's archive has no STRM_BGM_C_01");
    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path root = std::filesystem::path(outDir) / "load" / "mods" / id;
    std::filesystem::create_directories(root / "romfs" / "sound");
    std::filesystem::create_directories(scratchDir);

    std::vector<NewStreamSound> added(sounds.Streams.size());
    std::vector<std::string> notes(sounds.Streams.size());
    std::mutex romLock;
    const auto work = [&](size_t k) {
        const BcsarStream& s = sounds.Streams[k];
        const std::string base = s.File.substr(0, s.File.rfind('.'));
        added[k] = {base + ".dspadpcm.bcstm", s.Volume};
        Bytes aac;
        {
            std::lock_guard<std::mutex> hold(romLock);
            if (oras.Files().count("sound/" + added[k].File)) { notes[k] = s.File + ": Omega Ruby's own " + added[k].File + " played"; return; }
            aac = xy.Read("sound/" + s.File);
        }
        const Bytes bcstm = ConvertStream(aac, s, scratchDir, notes[k]);
        WriteFile((root / "romfs" / "sound" / added[k].File).string(), bcstm);
    };
    const size_t threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    std::vector<std::future<void>> running;
    for (size_t k = 0; k < sounds.Streams.size(); k++)
    {
        running.push_back(std::async(std::launch::async, work, k));
        if (running.size() >= threads) { running.front().get(); running.erase(running.begin()); }
    }
    for (auto& r : running) r.get();
    for (const std::string& n : notes) log.push_back("music: " + n);
    std::filesystem::remove_all(scratchDir);

    size_t first = 0;
    archive = BcsarAppendStreams(archive, like, added, first);
    if (first != sounds.First) throw FormatError("music: the archive's first new sound is not the one the ids were given");
    char line[160];
    snprintf(line, sizeof line, "music: %zu X/Y streams as sounds %zu-%zu", added.size(), first, first + added.size() - 1);
    log.push_back(line);
    return log;
}

}
