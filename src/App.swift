import SwiftUI
import AppKit

struct Snapshot: Decodable {
    let model: String
    let bridgeConnected: Bool
    let cpuTemperature: Double?
    let cpuFanRPM: Int?
    let gpuFanRPM: Int?
    let profileSupported: Bool
    let profileChangesEnabled: Bool
    let profile: Int?
    let logicalCPUs: Int
    let memoryGiB: Double
}

@MainActor final class FanModel: ObservableObject {
    @Published var snapshot: Snapshot?
    @Published var message = "正在读取温度与风扇状态…"
    @Published var busy = false
    private var timer: Timer?
    var helper: URL { Bundle.main.resourceURL!.appendingPathComponent("txfanctl") }
    func begin() {
        refresh()
        timer = Timer.scheduledTimer(withTimeInterval: 3, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
    }
    func refresh() {
        guard !busy else { return }
        busy = true
        let path = helper
        Task {
            let result = await Task.detached { () -> Snapshot? in
                let process = Process(), pipe = Pipe()
                process.executableURL = path; process.arguments = ["status"]
                process.standardOutput = pipe; process.standardError = FileHandle.nullDevice
                do {
                    try process.run()
                    let data = pipe.fileHandleForReading.readDataToEndOfFile()
                    process.waitUntilExit()
                    guard process.terminationStatus == 0 else { return nil }
                    return try JSONDecoder().decode(Snapshot.self, from: data)
                } catch { return nil }
            }.value
            self.snapshot = result
            if let result {
                if !result.bridgeConnected { self.message = "温度监控可用；风扇桥接驱动尚未加载。" }
                else if !result.profileSupported { self.message = "已连接华硕风扇接口；固件未确认模式切换支持。" }
                else if !result.profileChangesEnabled { self.message = "接口读取正常；选择运行模式时会请求管理员授权。" }
                else { self.message = "已连接华硕固件接口，可以切换运行模式。" }
            } else { self.message = "状态读取失败，请使用诊断命令检查。" }
            self.busy = false
        }
    }
    func select(_ profile: Int) {
        guard let state = snapshot, state.bridgeConnected, state.profileSupported,
              (0...2).contains(profile), !busy else { return }
        busy = true
        let command = "'" + helper.path.replacingOccurrences(of: "'", with: "'\\''") + "' profile \(profile)"
        let escaped = command.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
        let script = "do shell script \"\(escaped)\" with administrator privileges"
        Task {
            let errorText = await Task.detached { () -> String? in
                var error: NSDictionary?
                guard let appleScript = NSAppleScript(source: script) else { return "无法创建授权请求。" }
                appleScript.executeAndReturnError(&error)
                return error?[NSAppleScript.errorMessage] as? String
            }.value
            self.busy = false
            if let errorText { self.message = errorText }
            else { self.refresh() }
        }
    }
}

struct Dashboard: View {
    @ObservedObject var model: FanModel
    func card(_ title: String, _ value: String, _ icon: String) -> some View {
        VStack(alignment: .leading, spacing: 14) {
            Label(title, systemImage: icon).foregroundStyle(.secondary)
            Text(value).font(.system(size: 30, weight: .semibold, design: .rounded))
        }
        .frame(maxWidth: .infinity, alignment: .leading).padding(20)
        .background(Color.white.opacity(0.055), in: RoundedRectangle(cornerRadius: 14))
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 22) {
            HStack {
                VStack(alignment: .leading, spacing: 6) {
                    Text("TX Gaming 风扇助手").font(.system(size: 26, weight: .bold))
                    Text("FX608LM · Core Ultra 7 255HX").foregroundStyle(.secondary)
                }
                Spacer()
                Image(systemName: "fanblades.fill").font(.system(size: 38)).foregroundStyle(.cyan)
            }
            HStack(spacing: 14) {
                card("CPU 温度", model.snapshot?.cpuTemperature.map { "\(Int($0)) °C" } ?? "—", "thermometer.medium")
                card("CPU 风扇", model.snapshot?.cpuFanRPM.map { "\($0) RPM" } ?? "待验证", "fanblades")
                card("GPU 风扇", model.snapshot?.gpuFanRPM.map { "\($0) RPM" } ?? "待验证", "fanblades")
            }
            VStack(alignment: .leading, spacing: 14) {
                Text("固件运行模式").font(.headline)
                if let current = model.snapshot?.profile {
                    Text(current == 0 ? "当前：标准" : current == 1 ? "当前：性能" : current == 2 ? "当前：安静" : "当前：固件模式 \(current)")
                        .font(.callout).foregroundStyle(.secondary)
                }
                HStack(spacing: 12) {
                    ForEach([(2,"安静"),(0,"标准"),(1,"性能")], id: \.0) { profile in
                        Button { model.select(profile.0) } label: {
                            VStack(spacing: 7) {
                                Image(systemName: profile.0 == 2 ? "leaf" : profile.0 == 1 ? "bolt" : "gearshape")
                                Text(profile.1)
                            }.frame(maxWidth: .infinity).padding(.vertical, 14)
                        }
                        .buttonStyle(.bordered)
                        .tint(model.snapshot?.profile == profile.0 ? .cyan : .gray)
                        .disabled(model.busy || !(model.snapshot?.bridgeConnected ?? false) ||
                                  !(model.snapshot?.profileSupported ?? false))
                    }
                }
                Text("风扇继续由固件根据温度自动调速。运行模式也可能调整功耗与性能。")
                    .font(.callout).foregroundStyle(.secondary)
            }
            Divider()
            HStack(alignment: .top) {
                Image(systemName: model.snapshot?.bridgeConnected == true ? "checkmark.circle" : "info.circle")
                    .foregroundStyle(.cyan)
                Text(model.message).font(.callout).textSelection(.enabled)
                Spacer()
                Button("刷新") { model.refresh() }.disabled(model.busy)
            }
            if let state = model.snapshot {
                Text("\(state.logicalCPUs) 个逻辑核心 · \(Int(state.memoryGiB)) GB 内存")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
        .padding(28).frame(width: 720).preferredColorScheme(.dark)
    }
}

@main struct TXGamingFanApp: App {
    @StateObject var model = FanModel()
    var body: some Scene {
        WindowGroup("TX Gaming 风扇助手") {
            Dashboard(model: model).onAppear { model.begin() }
        }.windowResizability(.contentSize)
    }
}
