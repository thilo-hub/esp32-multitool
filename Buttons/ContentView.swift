//
//  ContentView.swift
//  Butttons
//
//  Created by Thilo Jeremias on 03.06.23.
//

import SwiftUI
//import MultipeerConnectivitimport Collections
import Collections

let ServerURL = URL(string: "http://beaglebone1.nispuk.com")!;


struct BlueButton: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .padding()
            .background(Color(red: 0, green: 0, blue: 0.5))
            .foregroundColor(.white)
            .clipShape(Capsule())
    }
}

struct iButton: View {
    var name: String
    var btn: String
    let url = ServerURL.appendingPathComponent("/send", conformingTo: .content)
    func post(button: String,state:Bool) {
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        let pdest = "buttons/"+btn + "." + (state ? "on":"off")
        let uploadData=Data(pdest.utf8)
        let task = URLSession.shared.uploadTask(with: request, from: uploadData) { data, response, error in
            if let error = error {
                print ("error: \(error)")
                return
            }
            guard let response = response as? HTTPURLResponse,
                (200...299).contains(response.statusCode) else {
                print ("server error")
                return
            }
            if let mimeType = response.mimeType,
                mimeType == "application/json",
                let data = data,
                let dataString = String(data: data, encoding: .utf8) {
                print ("got data: \(dataString)")
            }
        }
        task.resume()
    }
    var body: some View {
        HStack {
            Button(action: {post(button: name, state: true)}){
                Image("ButtonGreen")
//                    .imageScale(.large)
                    .resizable()
                    .aspectRatio(contentMode: .fit)
//                .frame(width: 60, height: 60, alignment: .topLeading)
                .clipped()

            }
//            .frame( maxWidth: .infinity,   alignment: .center)
            
            Text(name)
//                .dynamicTypeSize(.xxLarge)
                .frame(width: 150,height: 50)
            Button(action: {post(button: name, state: false)}){
                Image("ButtonGray")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
//                    .frame(width: 60, height: 60, alignment: .topLeading)

            }
//            .border(Color.gray)
//            .frame( maxWidth: .infinity,  maxHeight: .infinity, alignment: .center)
        }
//        .frame( maxWidth: .infinity,    alignment: .center)
        .frame( height: 60)
    }
}
struct ContentView: View {
    @State var Btns: OrderedDictionary<String,String> = [:]

    
    var body: some View {
        VStack {
            ForEach(Btns.keys, id: \.self) {
                iButton(name: Btns[$0]! ,btn:$0)
            }

        }

//        Spacer()
        .padding()
        .task {
                        do {
                            let url = ServerURL.appendingPathComponent("/buttons.json", conformingTo: .json)
                            
                            //URL(string: "http://beaglebone1/buttons.json")!;
                            let (data, _) = try await URLSession.shared.data(from: url)
                            let btn = try JSONSerialization.jsonObject(with: data, options: []) as? [String: String]
                            Btns = OrderedDictionary(uniqueKeysWithValues: btn!)
                                Btns.sort(by: { (f1, f2) -> Bool in
                                return f1.key < f2.key
                            })
                        } catch {
                            fatalError("Error thrown.")
                        }
                    }
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
