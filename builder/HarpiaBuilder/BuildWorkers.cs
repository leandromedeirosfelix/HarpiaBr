using System.Diagnostics;
using System.Globalization;

namespace HarpiaBuilder;

/// The build's speed, for this window only. Automatic leaves the build tools' own choice (one compiler per CPU thread while the
/// free memory holds it: nx2/build.py's MemoryGate). Maximum is every CPU thread with less memory kept in reserve; the memory
/// gate still holds compiles back before the PC runs out. A number is that many compilers.
static class BuildWorkers
{
    /// (local: Harpia) a named speed, shown in the window's language (Ui): "Automático" or "Automatic", not both
    public sealed record Speed(string Key)
    {
        public override string ToString() => Ui.L(Key);
    }
    public static readonly Speed Automatic = new("Automatic");
    public static readonly Speed Maximum = new("Maximum");
    const string MaximumReserveGb = "0.5";   // (Automatic keeps 1.5 GB free: HARPIA_MEM_RESERVE_GB in nx2/build.py)

    public static IEnumerable<object> Choices(int processors)
    {
        yield return Automatic;
        yield return Maximum;
        for (var count = 1; count <= Math.Max(1, processors); count++) yield return count;
    }

    /// null: Automatic; otherwise the compiler count (Maximum: every CPU thread)
    public static int? Selected(object? value) => value switch
    {
        Speed s when s == Automatic => null,
        Speed s when s == Maximum => Math.Max(1, Environment.ProcessorCount),
        int count when count > 0 => count,
        _ => throw new ArgumentException("Escolha Automático, Máximo ou um número de compiladores. / Choose Automatic, Maximum or a number of compilers."),
    };

    public static void Apply(ProcessStartInfo info, int? count)
    {
        if (count is <= 0) throw new ArgumentOutOfRangeException(nameof(count), "Build workers must be positive.");
        if (count is not int selected) return;
        info.Environment["HARPIA_JOBS"] = selected.ToString(CultureInfo.InvariantCulture);
        // (every thread, as Maximum or chosen by number: the smaller reserve is what lets them all run on a big CPU)
        if (selected >= Environment.ProcessorCount) info.Environment["HARPIA_MEM_RESERVE_GB"] = MaximumReserveGb;
    }
}
